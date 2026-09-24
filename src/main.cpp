#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <algorithm>
#include <cwctype>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "plugin2.h"

namespace {
constexpr wchar_t kWindowClass[] = L"HARULAB.SceneLibrary.Window";
constexpr wchar_t kWindowTitle[] = L"SceneShelf";
constexpr char kProjectKey[] = "harulab.scene_library.v1";
constexpr UINT kRefreshMessage = WM_APP + 73;
constexpr UINT kCreateFromSelectionMessage = WM_APP + 74;
constexpr COLORREF kBackground = RGB(45, 45, 45);
constexpr COLORREF kSurface = RGB(37, 37, 37);
constexpr COLORREF kButton = RGB(72, 72, 72);
constexpr COLORREF kButtonPressed = RGB(92, 92, 92);
constexpr COLORREF kBorder = RGB(104, 104, 104);
constexpr COLORREF kText = RGB(235, 235, 235);
constexpr int kSearch = 100, kTree = 101, kAdd = 106, kCreateScene = 107;
constexpr int kMenuNewFolder = 201, kMenuRenameFolder = 202, kMenuDeleteFolder = 203,
              kMenuOpenScene = 204, kMenuRenameScene = 205,
              kMenuSettingsScene = 206, kMenuDeleteScene = 207;

struct Scene { int id; std::wstring name; };
struct Node { bool folder; int scene_id; std::wstring path; };
struct ObjectSnapshot { std::string alias; int layer, start, end; };
struct SelectionSnapshot {
  EDIT_INFO info{};
  std::vector<ObjectSnapshot> objects;
  int expected = 0;
  bool complete = true;
};
struct State {
  HWND window{}, tree{}, search{};
  EDIT_HANDLE* edit{};
  std::vector<Scene> scenes;
  std::set<std::wstring> folders;
  std::map<int, std::wstring> assignments;
  std::map<std::wstring, std::vector<int>> orders;
  std::vector<std::unique_ptr<Node>> nodes;
  int drag_scene_id = -1;
  UINT dpi = 96;
  bool dirty{};
  HFONT font{};
  HIMAGELIST images{};
  int folder_image = -1, scene_image = -1;
  HBRUSH background_brush{}, surface_brush{};
} g;

std::string utf8(const std::wstring& value) {
  if (value.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, value.data(), (int)value.size(), nullptr, 0, nullptr, nullptr);
  std::string result(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.data(), (int)value.size(), result.data(), n, nullptr, nullptr);
  return result;
}
std::wstring wide(const std::string& value) {
  if (value.empty()) return {};
  int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), (int)value.size(), nullptr, 0);
  if (!n) return {};
  std::wstring result(n, L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), (int)value.size(), result.data(), n);
  return result;
}
std::string hex(const std::string& value) {
  static constexpr char digits[] = "0123456789ABCDEF";
  std::string result;
  for (unsigned char c : value) { result += digits[c >> 4]; result += digits[c & 15]; }
  return result;
}
std::string unhex(const std::string& value) {
  if (value.size() % 2) return {};
  auto digit = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
  };
  std::string result;
  for (size_t i = 0; i < value.size(); i += 2) {
    int a = digit(value[i]), b = digit(value[i + 1]);
    if (a < 0 || b < 0) return {};
    result += char((a << 4) | b);
  }
  return result;
}
std::wstring text(HWND control) {
  int n = GetWindowTextLengthW(control);
  std::wstring result(n + 1, L'\0');
  GetWindowTextW(control, result.data(), n + 1);
  result.resize(n);
  return result;
}
int px(int value) { return MulDiv(value, static_cast<int>(g.dpi), 96); }
void apply_images() {
  HIMAGELIST previous = g.images;
  g.images = ImageList_Create(px(16), px(16), ILC_COLOR32 | ILC_MASK, 2, 1);
  g.folder_image = g.scene_image = -1;
  SHFILEINFOW info{};
  if (g.images && SHGetFileInfoW(L"folder", FILE_ATTRIBUTE_DIRECTORY, &info, sizeof(info),
                                 SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES)) {
    g.folder_image = ImageList_AddIcon(g.images, info.hIcon);
    DestroyIcon(info.hIcon);
  }
  if (g.images && SHGetFileInfoW(L"scene.aup2", FILE_ATTRIBUTE_NORMAL, &info, sizeof(info),
                                 SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES)) {
    g.scene_image = ImageList_AddIcon(g.images, info.hIcon);
    DestroyIcon(info.hIcon);
  }
  if (g.tree) TreeView_SetImageList(g.tree, g.images, TVSIL_NORMAL);
  if (previous) ImageList_Destroy(previous);
}
void apply_font(int dpi) {
  g.dpi = dpi ? static_cast<UINT>(dpi) : 96;
  HFONT previous = g.font;
  g.font = CreateFontW(-MulDiv(8, static_cast<int>(g.dpi), 72), 0, 0, 0, FW_NORMAL,
                       FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                       L"Yu Gothic UI");
  for (HWND control : {g.search, g.tree, GetDlgItem(g.window, kAdd),
                       GetDlgItem(g.window, kCreateScene)}) {
    if (control) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g.font), TRUE);
  }
  if (g.tree) TreeView_SetItemHeight(g.tree, px(16));
  if (g.tree) apply_images();
  if (previous) DeleteObject(previous);
}
void mark_dirty() {
  g.dirty = true;
  if (g.edit) g.edit->call_edit_section([](EDIT_SECTION* edit) { edit->set_edited_state(); });
}
std::string serialize() {
  std::string out = "v3";
  for (const auto& path : g.folders) out += ";F:" + hex(utf8(path));
  for (const auto& [id, path] : g.assignments)
    out += ";S:" + std::to_string(id) + ":" + hex(utf8(path));
  for (const auto& [path, ids] : g.orders) {
    out += ";O:" + hex(utf8(path)) + ":";
    for (size_t i = 0; i < ids.size(); ++i) {
      if (i) out += ",";
      out += std::to_string(ids[i]);
    }
  }
  return out;
}
void deserialize(const char* raw) {
  g.folders.clear(); g.assignments.clear(); g.orders.clear(); g.dirty = false;
  if (!raw) return;
  std::string data(raw);
  if (data.rfind("v2", 0) != 0 && data.rfind("v3", 0) != 0) return;
  size_t pos = 2;
  while (pos < data.size()) {
    if (data[pos] != ';') break;
    ++pos;
    size_t end = data.find(';', pos);
    if (end == std::string::npos) end = data.size();
    auto token = data.substr(pos, end - pos);
    if (token.rfind("F:", 0) == 0) {
      auto path = wide(unhex(token.substr(2)));
      if (!path.empty()) g.folders.insert(path);
    } else if (token.rfind("S:", 0) == 0) {
      size_t colon = token.find(':', 2);
      if (colon != std::string::npos) {
        try { g.assignments[std::stoi(token.substr(2, colon - 2))] = wide(unhex(token.substr(colon + 1))); }
        catch (...) {}
      }
    } else if (token.rfind("O:", 0) == 0) {
      size_t colon = token.find(':', 2);
      if (colon != std::string::npos) {
        auto path = wide(unhex(token.substr(2, colon - 2)));
        auto& ids = g.orders[path];
        size_t start = colon + 1;
        while (start < token.size()) {
          size_t comma = token.find(',', start);
          if (comma == std::string::npos) comma = token.size();
          try { ids.push_back(std::stoi(token.substr(start, comma - start))); }
          catch (...) {}
          start = comma + 1;
        }
      }
    }
    pos = end;
  }
}
void project_load(PROJECT_FILE* project) {
  deserialize(project->get_param_string(kProjectKey));
  g.drag_scene_id = -1;
  if (g.window) PostMessageW(g.window, kRefreshMessage, 0, 0);
}
void project_save(PROJECT_FILE* project) {
  auto data = serialize();
  project->set_param_string(kProjectKey, data.c_str());
  g.dirty = false;
}
void refresh_scenes() {
  if (!g.edit) return;
  std::vector<Scene> found;
  g.edit->enum_scene_name(&found, [](void* param, LPCWSTR name, int id) {
    static_cast<std::vector<Scene>*>(param)->push_back({id, name ? name : L""});
  });
  if (found.size() == g.scenes.size() && std::equal(found.begin(), found.end(), g.scenes.begin(),
      [](const Scene& a, const Scene& b) { return a.id == b.id && a.name == b.name; })) return;
  g.scenes = std::move(found);
  PostMessageW(g.window, kRefreshMessage, 0, 0);
}
HTREEITEM insert_node(HTREEITEM parent, const std::wstring& label, bool folder, int id, const std::wstring& path) {
  auto node = std::make_unique<Node>(Node{folder, id, path});
  TVINSERTSTRUCTW item{};
  item.hParent = parent;
  item.hInsertAfter = TVI_LAST;
  item.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
  item.item.pszText = const_cast<LPWSTR>(label.c_str());
  item.item.lParam = reinterpret_cast<LPARAM>(node.get());
  item.item.iImage = folder ? g.folder_image : g.scene_image;
  item.item.iSelectedImage = folder ? g.folder_image : g.scene_image;
  HTREEITEM handle = TreeView_InsertItem(g.tree, &item);
  g.nodes.push_back(std::move(node));
  return handle;
}
void rebuild_tree() {
  if (!g.tree) return;
  TreeView_DeleteAllItems(g.tree);
  g.nodes.clear();
  std::wstring query = text(g.search);
  std::transform(query.begin(), query.end(), query.begin(), towlower);
  std::map<std::wstring, HTREEITEM> handles;
  handles[L""] = TVI_ROOT;
  for (const auto& folder : g.folders) {
    size_t slash = folder.rfind(L'\\');
    auto parent_path = slash == std::wstring::npos ? L"" : folder.substr(0, slash);
    auto label = slash == std::wstring::npos ? folder : folder.substr(slash + 1);
    auto it = handles.find(parent_path);
    handles[folder] = insert_node(it == handles.end() ? TVI_ROOT : it->second, label, true, -1, folder);
  }
  std::map<std::wstring, std::vector<const Scene*>> grouped;
  for (const auto& scene : g.scenes) {
    auto lower = scene.name;
    std::transform(lower.begin(), lower.end(), lower.begin(), towlower);
    if (!query.empty() && lower.find(query) == std::wstring::npos) continue;
    auto assigned = g.assignments.find(scene.id);
    auto path = assigned == g.assignments.end() ? L"" : assigned->second;
    grouped[path].push_back(&scene);
  }
  for (auto& [path, scenes] : grouped) {
    auto order = g.orders.find(path);
    if (order != g.orders.end()) {
      std::map<int, size_t> rank;
      for (size_t i = 0; i < order->second.size(); ++i) rank.emplace(order->second[i], i);
      std::stable_sort(scenes.begin(), scenes.end(), [&](const Scene* a, const Scene* b) {
        auto ra = rank.find(a->id), rb = rank.find(b->id);
        if (ra == rank.end()) return false;
        if (rb == rank.end()) return true;
        return ra->second < rb->second;
      });
    }
    auto it = handles.find(path);
    for (const auto* scene : scenes)
      insert_node(it == handles.end() ? TVI_ROOT : it->second, scene->name, false, scene->id, path);
  }
  for (const auto& [path, handle] : handles) if (!path.empty()) TreeView_Expand(g.tree, handle, TVE_EXPAND);
}
void remove_from_orders(int scene_id) {
  for (auto& [path, ids] : g.orders)
    ids.erase(std::remove(ids.begin(), ids.end(), scene_id), ids.end());
}
void place_in_order(int scene_id, const std::wstring& path, int target_id, bool after) {
  auto& ids = g.orders[path];
  if (ids.empty()) {
    for (const auto& scene : g.scenes) {
      auto assigned = g.assignments.find(scene.id);
      auto scene_path = assigned == g.assignments.end() ? L"" : assigned->second;
      if (scene_path == path) ids.push_back(scene.id);
    }
  }
  ids.erase(std::remove(ids.begin(), ids.end(), scene_id), ids.end());
  auto at = std::find(ids.begin(), ids.end(), target_id);
  if (at == ids.end()) ids.push_back(scene_id);
  else ids.insert(at + (after ? 1 : 0), scene_id);
}
Node* node_at(HTREEITEM handle) {
  if (!handle) return nullptr;
  TVITEMW item{}; item.mask = TVIF_PARAM; item.hItem = handle;
  return TreeView_GetItem(g.tree, &item) ? reinterpret_cast<Node*>(item.lParam) : nullptr;
}
Node* selected() {
  return node_at(TreeView_GetSelection(g.tree));
}
HTREEITEM drop_target(POINT point, bool* inside, bool* after) {
  MapWindowPoints(g.window, g.tree, &point, 1);
  RECT area{}; GetClientRect(g.tree, &area);
  *inside = PtInRect(&area, point) != FALSE;
  if (!*inside) return nullptr;
  TVHITTESTINFO hit{}; hit.pt = point;
  HTREEITEM item = TreeView_HitTest(g.tree, &hit);
  RECT row{};
  if (item && TreeView_GetItemRect(g.tree, item, &row, TRUE))
    *after = point.y >= row.top + (row.bottom - row.top) / 2;
  else *after = true;
  return item;
}
void layout(HWND hwnd) {
  RECT r{}; GetClientRect(hwnd, &r);
  int w = r.right, h = r.bottom;
  const int margin = px(6), gap = px(4), search_h = px(20);
  const int button_h = px(20);
  const int button_y = h - margin - button_h;
  const int search_y = button_y - gap - search_h;
  MoveWindow(g.search, margin, search_y, w - margin * 2, search_h, TRUE);
  MoveWindow(g.tree, margin, margin, w - margin * 2,
             std::max(px(40), search_y - gap - margin), TRUE);
  const int bw = std::max(px(60), (w - margin * 2 - gap) / 2);
  MoveWindow(GetDlgItem(hwnd, kAdd), margin, button_y, bw, button_h, TRUE);
  MoveWindow(GetDlgItem(hwnd, kCreateScene), margin + bw + gap, button_y, bw, button_h, TRUE);
}
HWND button(HWND hwnd, int id, LPCWSTR label) {
  return CreateWindowExW(0, WC_BUTTONW, label, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                         0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
}
void new_folder() {
  auto* node = selected();
  std::wstring parent = node && node->folder ? node->path : L"";
  std::wstring base = parent.empty() ? L"新しいフォルダ" : parent + L"\\新しいフォルダ";
  std::wstring name = base;
  for (int i = 2; g.folders.count(name); ++i) name = base + L" " + std::to_wstring(i);
  g.folders.insert(name); mark_dirty(); rebuild_tree();
  for (HTREEITEM h = TreeView_GetRoot(g.tree); h; h = TreeView_GetNextSibling(g.tree, h)) {
    std::vector<HTREEITEM> stack{h};
    while (!stack.empty()) {
      HTREEITEM current = stack.back(); stack.pop_back();
      TVITEMW item{}; item.mask = TVIF_PARAM; item.hItem = current;
      if (TreeView_GetItem(g.tree, &item)) {
        auto* current_node = reinterpret_cast<Node*>(item.lParam);
        if (current_node && current_node->folder && current_node->path == name) {
          TreeView_SelectItem(g.tree, current);
          TreeView_EditLabel(g.tree, current);
          return;
        }
      }
      for (HTREEITEM child = TreeView_GetChild(g.tree, current); child;
           child = TreeView_GetNextSibling(g.tree, child)) stack.push_back(child);
    }
  }
}
void delete_folder() {
  auto* node = selected();
  if (!node || !node->folder || node->path.empty()) return;
  std::wstring oldpath = node->path;
  if (MessageBoxW(g.window, L"このフォルダとサブフォルダを削除しますか？\n中のシーンは親フォルダへ移動します。",
                  kWindowTitle, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return;
  size_t slash = oldpath.rfind(L'\\');
  std::wstring parent = slash == std::wstring::npos ? L"" : oldpath.substr(0, slash);
  for (auto it = g.folders.begin(); it != g.folders.end();) {
    if (*it == oldpath || it->rfind(oldpath + L"\\", 0) == 0) it = g.folders.erase(it);
    else ++it;
  }
  for (auto it = g.assignments.begin(); it != g.assignments.end();) {
    if (it->second == oldpath || it->second.rfind(oldpath + L"\\", 0) == 0) {
      if (parent.empty()) it = g.assignments.erase(it);
      else { it->second = parent; ++it; }
    } else ++it;
  }
  mark_dirty(); rebuild_tree();
}
void open_scene() {
  auto* node = selected();
  if (!node || node->folder) return;
  if (g.edit) g.edit->select_scene(node->scene_id);
}
bool find_menu_command(HMENU menu, const std::wstring& expected, UINT* command) {
  if (!menu) return false;
  const int count = GetMenuItemCount(menu);
  for (int i = 0; i < count; ++i) {
    MENUITEMINFOW item{};
    item.cbSize = sizeof(item);
    item.fMask = MIIM_ID | MIIM_SUBMENU | MIIM_STRING;
    wchar_t label[256]{};
    item.dwTypeData = label;
    item.cch = static_cast<UINT>(std::size(label));
    if (!GetMenuItemInfoW(menu, i, TRUE, &item)) continue;
    std::wstring text = label;
    if (auto tab = text.find(L'\t'); tab != std::wstring::npos) text.resize(tab);
    if (!text.empty() && text == expected && item.wID != 0) {
      *command = item.wID;
      return true;
    }
    if (item.hSubMenu && find_menu_command(item.hSubMenu, expected, command)) return true;
  }
  return false;
}
bool invoke_host_scene_command(int scene_id, const wchar_t* label) {
  if (!g.edit || !g.edit->select_scene(scene_id)) return false;
  HWND host = g.edit->get_host_app_window();
  UINT command = 0;
  if (!host || !find_menu_command(GetMenu(host), label, &command)) return false;
  SendMessageW(host, WM_COMMAND, MAKEWPARAM(command, 0), 0);
  return true;
}
void rename_selected() {
  auto* node = selected();
  if (!node) return;
  if (node->folder) {
    if (!node->path.empty()) TreeView_EditLabel(g.tree, TreeView_GetSelection(g.tree));
    return;
  }
  if (!invoke_host_scene_command(node->scene_id, L"現在のシーン名を変更"))
    MessageBoxW(g.window, L"AviUtl2のシーン名変更メニューを見つけられませんでした。",
                kWindowTitle, MB_OK | MB_ICONWARNING);
}
void open_scene_settings() {
  auto* node = selected();
  if (node && !node->folder &&
      !invoke_host_scene_command(node->scene_id, L"現在のシーンを設定"))
    MessageBoxW(g.window, L"AviUtl2のシーン設定メニューを見つけられませんでした。",
                kWindowTitle, MB_OK | MB_ICONWARNING);
}
void delete_selected_scene() {
  auto* node = selected();
  if (!node || node->folder) return;
  const int scene_id = node->scene_id;
  if (MessageBoxW(g.window, L"選択したシーンを削除します。よろしいですか？",
                  kWindowTitle, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return;
  if (!invoke_host_scene_command(scene_id, L"現在のシーンを削除")) {
    MessageBoxW(g.window, L"AviUtl2のシーン削除メニューを見つけられませんでした。",
                kWindowTitle, MB_OK | MB_ICONWARNING);
    return;
  }
  refresh_scenes();
  if (std::none_of(g.scenes.begin(), g.scenes.end(),
                   [scene_id](const Scene& scene) { return scene.id == scene_id; })) {
    g.assignments.erase(scene_id);
    remove_from_orders(scene_id);
    mark_dirty();
    rebuild_tree();
  }
}
struct AddRequest { int id, layer, frame; bool at_drop, created; };
void insert_scene_reference(int scene_id, const POINT* screen = nullptr) {
  if (!g.edit) return;
  EDIT_INFO info{};
  g.edit->get_edit_info(&info, sizeof(info));
  if (info.scene_id == scene_id) return;
  AddRequest request{scene_id, 0, 0, screen != nullptr, false};
  if (screen) { request.layer = screen->x; request.frame = screen->y; }
  g.edit->call_edit_section_param(&request, [](void* raw, EDIT_SECTION* edit) {
    auto& req = *static_cast<AddRequest*>(raw);
    int layer = edit->info->layer, frame = edit->info->frame;
    if (req.at_drop && !edit->pos_to_layer_frame(req.layer, req.frame, &layer, &frame)) return;
    std::string alias = "[Object]\n[Object.0]\neffect.name=シーン\nシーン=" + std::to_string(req.id) +
                        "\n[Object.1]\neffect.name=映像再生\n";
    req.created = edit->create_object_from_alias(alias.c_str(), layer, frame, 150) != nullptr;
  });
}
void add_scene() {
  auto* node = selected();
  if (node && !node->folder) insert_scene_reference(node->scene_id);
}
bool capture_selection(SelectionSnapshot& snapshot) {
  if (!g.edit) return false;
  g.edit->get_edit_info(&snapshot.info, sizeof(snapshot.info));
  if (snapshot.info.width <= 0 || snapshot.info.height <= 0 ||
      snapshot.info.rate <= 0 || snapshot.info.scale <= 0) return false;
  if (!g.edit->call_read_section_param(&snapshot, [](void* raw, EDIT_SECTION* edit) {
    auto& result = *static_cast<SelectionSnapshot*>(raw);
    result.expected = edit->get_selected_object_num();
    for (int i = 0; i < result.expected; ++i) {
      OBJECT_HANDLE object = edit->get_selected_object(i);
      if (!object) { result.complete = false; break; }
      auto location = edit->get_object_layer_frame(object);
      const char* alias = edit->get_object_alias(object);
      if (!alias || !*alias || location.end < location.start) { result.complete = false; break; }
      result.objects.push_back({alias, location.layer, location.start, location.end});
    }
  })) return false;
  return snapshot.complete && static_cast<int>(snapshot.objects.size()) == snapshot.expected;
}
void create_scene_from_snapshot(const SelectionSnapshot& snapshot) {
  if (!g.edit) return;
  const auto& info = snapshot.info;
  if (info.width <= 0 || info.height <= 0 || info.rate <= 0 || info.scale <= 0) return;
  EDIT_INFO current{};
  g.edit->get_edit_info(&current, sizeof(current));
  if (current.scene_id != info.scene_id) return;
  std::wstring folder;
  if (auto* node = selected(); node && node->folder) folder = node->path;
  std::set<int> before;
  for (const auto& scene : g.scenes) before.insert(scene.id);
  std::wstring name = L"新しいシーン " + std::to_wstring(g.scenes.size() + 1);
  if (!g.edit->create_scene(name.c_str(), nullptr, info.width, info.height,
                            info.rate, info.scale, info.sample_rate, info.background)) {
    return;
  }
  if (!snapshot.objects.empty()) {
    struct PasteRequest { const SelectionSnapshot* snapshot; int created; } paste{&snapshot, 0};
    g.edit->call_edit_section_param(&paste, [](void* raw, EDIT_SECTION* edit) {
      auto& req = *static_cast<PasteRequest*>(raw);
      const auto& objects = req.snapshot->objects;
      const int first_frame = std::min_element(objects.begin(), objects.end(),
          [](const auto& a, const auto& b) { return a.start < b.start; })->start;
      const int first_layer = std::min_element(objects.begin(), objects.end(),
          [](const auto& a, const auto& b) { return a.layer < b.layer; })->layer;
      for (const auto& object : objects) {
        if (!edit->create_object_from_alias(object.alias.c_str(), object.layer - first_layer,
                                             object.start - first_frame, object.end - object.start + 1)) break;
        ++req.created;
      }
    });
    if (paste.created != static_cast<int>(snapshot.objects.size()))
      MessageBoxW(g.window, L"一部のオブジェクトを新規シーンへコピーできませんでした。元のオブジェクトは変更していません。",
                  kWindowTitle, MB_OK | MB_ICONWARNING);
  }
  refresh_scenes();
  if (!folder.empty()) {
    for (const auto& scene : g.scenes) {
      if (!before.count(scene.id)) { g.assignments[scene.id] = folder; mark_dirty(); break; }
    }
    rebuild_tree();
  }
}
void create_scene() {
  SelectionSnapshot snapshot;
  if (capture_selection(snapshot)) create_scene_from_snapshot(snapshot);
  else MessageBoxW(g.window, L"選択オブジェクトの情報を取得できませんでした。シーンは作成していません。",
                   kWindowTitle, MB_OK | MB_ICONWARNING);
}
void create_scene_from_object_menu(void*) {
  auto snapshot = std::make_unique<SelectionSnapshot>();
  if (!capture_selection(*snapshot) || snapshot->objects.empty()) return;
  if (g.window && PostMessageW(g.window, kCreateFromSelectionMessage, 0,
                               reinterpret_cast<LPARAM>(snapshot.get()))) snapshot.release();
}
LRESULT CALLBACK tree_subclass_proc(HWND tree, UINT msg, WPARAM wp, LPARAM lp,
                                    UINT_PTR, DWORD_PTR) {
  if (msg == WM_LBUTTONDBLCLK) {
    TVHITTESTINFO hit{};
    hit.pt = POINT{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    HTREEITEM item = TreeView_HitTest(tree, &hit);
    TreeView_SelectItem(tree, item);
    auto* node = node_at(item);
    if (node && node->folder && !node->path.empty()) {
      TreeView_EditLabel(tree, item);
      return 0;
    }
    if (node && !node->folder) {
      open_scene();
      return 0;
    }
  }
  return DefSubclassProc(tree, msg, wp, lp);
}
LRESULT CALLBACK label_edit_subclass_proc(HWND edit, UINT msg, WPARAM wp, LPARAM lp,
                                          UINT_PTR subclass, DWORD_PTR tree_ref) {
  HWND tree = reinterpret_cast<HWND>(tree_ref);
  if (msg == WM_KEYDOWN && wp == VK_RETURN) {
    TreeView_EndEditLabelNow(tree, FALSE);
    return 0;
  }
  if (msg == WM_KEYDOWN && wp == VK_ESCAPE) {
    TreeView_EndEditLabelNow(tree, TRUE);
    return 0;
  }
  if (msg == WM_NCDESTROY) RemoveWindowSubclass(edit, label_edit_subclass_proc, subclass);
  return DefSubclassProc(edit, msg, wp, lp);
}
LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
  case WM_CREATE: {
    InitCommonControls();
    g.window = hwnd;
    g.background_brush = CreateSolidBrush(kBackground);
    g.surface_brush = CreateSolidBrush(kSurface);
    g.search = CreateWindowExW(WS_EX_CLIENTEDGE, WC_EDITW, L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                               0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)kSearch, GetModuleHandleW(nullptr), nullptr);
    // Keep the bright hierarchy connectors, but remove the enclosing 3D edge around the whole tree.
    g.tree = CreateWindowExW(0, WC_TREEVIEWW, L"", WS_CHILD | WS_VISIBLE | TVS_HASBUTTONS |
                            TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_EDITLABELS,
                            0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)kTree, GetModuleHandleW(nullptr), nullptr);
    SetWindowSubclass(g.tree, tree_subclass_proc, 1, 0);
    SetWindowTheme(g.search, L"", L"");
    SetWindowTheme(g.tree, L"", L"");
    TreeView_SetBkColor(g.tree, kSurface);
    TreeView_SetTextColor(g.tree, kText);
    TreeView_SetLineColor(g.tree, RGB(255, 255, 255));
    button(hwnd, kAdd, L"タイムライン追加"); button(hwnd, kCreateScene, L"新規シーン");
    apply_font(GetDpiForWindow(hwnd));
    SetTimer(hwnd, 1, 1500, nullptr);
    return 0;
  }
  case WM_ERASEBKGND: {
    RECT area{}; GetClientRect(hwnd, &area);
    FillRect(reinterpret_cast<HDC>(wp), &area, g.background_brush);
    return 1;
  }
  case WM_CTLCOLOREDIT: {
    HDC dc = reinterpret_cast<HDC>(wp);
    SetTextColor(dc, kText);
    SetBkColor(dc, kSurface);
    return reinterpret_cast<LRESULT>(g.surface_brush);
  }
  case WM_DRAWITEM: {
    auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lp);
    if (!item || item->CtlType != ODT_BUTTON) break;
    HBRUSH fill = CreateSolidBrush((item->itemState & ODS_SELECTED) ? kButtonPressed : kButton);
    FillRect(item->hDC, &item->rcItem, fill);
    DeleteObject(fill);
    FrameRect(item->hDC, &item->rcItem, g.surface_brush);
    wchar_t label[128]{};
    GetWindowTextW(item->hwndItem, label, 128);
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, (item->itemState & ODS_DISABLED) ? kBorder : kText);
    DrawTextW(item->hDC, label, -1, &item->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (item->itemState & ODS_FOCUS) DrawFocusRect(item->hDC, &item->rcItem);
    return TRUE;
  }
  case WM_SIZE: layout(hwnd); return 0;
  case WM_DPICHANGED:
    apply_font(LOWORD(wp));
    layout(hwnd);
    return 0;
  case WM_MOUSEMOVE:
    if (g.drag_scene_id >= 0) {
      bool inside = false;
      bool after = false;
      HTREEITEM target = drop_target(POINT{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}, &inside, &after);
      TreeView_SelectDropTarget(g.tree, target);
      SetCursor(LoadCursorW(nullptr, inside ? IDC_SIZEALL : IDC_ARROW));
      return 0;
    }
    break;
  case WM_LBUTTONUP:
    if (g.drag_scene_id >= 0) {
      const int id = g.drag_scene_id;
      POINT screen{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
      ClientToScreen(hwnd, &screen);
      bool inside = false;
      bool after = false;
      HTREEITEM target = drop_target(POINT{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}, &inside, &after);
      auto* target_node = node_at(target);
      g.drag_scene_id = -1;
      TreeView_SelectDropTarget(g.tree, nullptr);
      ReleaseCapture();
      if (inside) {
        std::wstring path = target_node ? target_node->path : L"";
        if (target_node && !target_node->folder && target_node->scene_id == id) {
          return 0;
        } else if (target_node && target_node->folder) {
          g.assignments[id] = path;
          remove_from_orders(id);
          place_in_order(id, path, -1, true);
        } else {
          if (path.empty()) g.assignments.erase(id);
          else g.assignments[id] = path;
          remove_from_orders(id);
          place_in_order(id, path, target_node ? target_node->scene_id : -1, after);
        }
        mark_dirty(); rebuild_tree();
      } else insert_scene_reference(id, &screen);
      return 0;
    }
    break;
  case WM_CAPTURECHANGED:
    if (g.drag_scene_id >= 0) {
      g.drag_scene_id = -1;
      TreeView_SelectDropTarget(g.tree, nullptr);
    }
    break;
  case WM_TIMER: refresh_scenes(); return 0;
  case kRefreshMessage: rebuild_tree(); return 0;
  case kCreateFromSelectionMessage: {
    std::unique_ptr<SelectionSnapshot> snapshot(reinterpret_cast<SelectionSnapshot*>(lp));
    if (snapshot) create_scene_from_snapshot(*snapshot);
    return 0;
  }
  case WM_CONTEXTMENU:
    if (reinterpret_cast<HWND>(wp) == g.tree) {
      POINT screen{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
      if (screen.x == -1 && screen.y == -1) GetCursorPos(&screen);
      POINT client = screen;
      ScreenToClient(g.tree, &client);
      TVHITTESTINFO hit{}; hit.pt = client;
      HTREEITEM item = TreeView_HitTest(g.tree, &hit);
      TreeView_SelectItem(g.tree, item);
      HMENU menu = CreatePopupMenu();
      AppendMenuW(menu, MF_STRING, kMenuNewFolder, L"フォルダを作成");
      if (auto* node = node_at(item); node && node->folder) {
        AppendMenuW(menu, MF_STRING, kMenuRenameFolder, L"名前を変更");
        AppendMenuW(menu, MF_STRING, kMenuDeleteFolder, L"フォルダを削除");
      } else if (auto* node = node_at(item); node && !node->folder) {
        AppendMenuW(menu, MF_STRING, kMenuOpenScene, L"シーンを開く");
        AppendMenuW(menu, MF_STRING, kMenuRenameScene, L"シーン名を変更");
        AppendMenuW(menu, MF_STRING, kMenuSettingsScene, L"シーンを設定");
        AppendMenuW(menu, MF_STRING, kMenuDeleteScene, L"シーンを削除");
      }
      int command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                   screen.x, screen.y, 0, hwnd, nullptr);
      DestroyMenu(menu);
      if (command) SendMessageW(hwnd, WM_COMMAND, command, 0);
      return 0;
    }
    break;
  case WM_COMMAND:
    if (LOWORD(wp) == kSearch && HIWORD(wp) == EN_CHANGE) { rebuild_tree(); return 0; }
    switch (LOWORD(wp)) {
    case kMenuNewFolder: new_folder(); return 0;
    case kMenuRenameFolder:
      if (auto* node = selected(); node && node->folder)
        TreeView_EditLabel(g.tree, TreeView_GetSelection(g.tree));
      return 0;
    case kMenuDeleteFolder: delete_folder(); return 0;
    case kMenuOpenScene: open_scene(); return 0;
    case kMenuRenameScene: rename_selected(); return 0;
    case kMenuSettingsScene: open_scene_settings(); return 0;
    case kMenuDeleteScene: delete_selected_scene(); return 0;
    case kCreateScene: create_scene(); return 0;
    case kAdd: add_scene(); return 0;
    }
    break;
  case WM_NOTIFY: {
    auto* h = reinterpret_cast<NMHDR*>(lp);
    if (h->idFrom == kTree && h->code == TVN_BEGINDRAGW) {
      auto* drag = reinterpret_cast<NMTREEVIEWW*>(lp);
      auto* node = node_at(drag->itemNew.hItem);
      if (node && !node->folder) {
        g.drag_scene_id = node->scene_id;
        SetCapture(hwnd);
      }
      return 0;
    }
    if (h->idFrom == kTree && h->code == TVN_KEYDOWN) {
      auto* key = reinterpret_cast<NMTVKEYDOWN*>(lp);
      if (key->wVKey == VK_F2) {
        rename_selected();
        return 0;
      }
      if (key->wVKey == VK_DELETE) {
        auto* node = selected();
        if (node && node->folder) delete_folder();
        else delete_selected_scene();
        return 0;
      }
    }
    if (h->idFrom == kTree && h->code == TVN_BEGINLABELEDITW) {
      HWND edit = TreeView_GetEditControl(g.tree);
      if (edit) SetWindowSubclass(edit, label_edit_subclass_proc, 2,
                                  reinterpret_cast<DWORD_PTR>(g.tree));
      return FALSE;
    }
    if (h->idFrom == kTree && h->code == TVN_ENDLABELEDITW) {
      auto* p = reinterpret_cast<NMTVDISPINFOW*>(lp);
      if (!p->item.pszText || !*p->item.pszText) return FALSE;
      auto* node = node_at(p->item.hItem);
      std::wstring label = p->item.pszText;
      if (label.find_first_of(L"\r\n\t") != std::wstring::npos) return FALSE;
      if (!node || !node->folder || node->path.empty() ||
          label.find_first_of(L"\\/") != std::wstring::npos) return FALSE;
      size_t slash = node->path.rfind(L'\\');
      std::wstring parent = slash == std::wstring::npos ? L"" : node->path.substr(0, slash + 1);
      std::wstring oldpath = node->path, newpath = parent + label;
      if (oldpath == newpath || g.folders.count(newpath)) return FALSE;
      std::set<std::wstring> renamed;
      for (const auto& path : g.folders)
        renamed.insert(path == oldpath || path.rfind(oldpath + L"\\", 0) == 0 ? newpath + path.substr(oldpath.size()) : path);
      g.folders = std::move(renamed);
      for (auto& [id, path] : g.assignments)
        if (path == oldpath || path.rfind(oldpath + L"\\", 0) == 0) path = newpath + path.substr(oldpath.size());
      std::map<std::wstring, std::vector<int>> renamed_orders;
      for (auto& [path, ids] : g.orders)
        renamed_orders[path == oldpath || path.rfind(oldpath + L"\\", 0) == 0
                           ? newpath + path.substr(oldpath.size()) : path] = std::move(ids);
      g.orders = std::move(renamed_orders);
      mark_dirty(); PostMessageW(hwnd, kRefreshMessage, 0, 0); return TRUE;
    }
    break;
  }
  case WM_DESTROY:
    KillTimer(hwnd, 1);
    DeleteObject(g.background_brush); g.background_brush = nullptr;
    DeleteObject(g.surface_brush); g.surface_brush = nullptr;
    if (g.font) DeleteObject(g.font);
    g.font = nullptr;
    if (g.tree) TreeView_SetImageList(g.tree, nullptr, TVSIL_NORMAL);
    if (g.images) ImageList_Destroy(g.images);
    g.images = nullptr;
    g.window = nullptr; g.tree = nullptr; return 0;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}
COMMON_PLUGIN_TABLE table{L"SceneShelf", L"Scene organizer for AviUtl2"};
}

extern "C" __declspec(dllexport) DWORD RequiredVersion() { return 2011000; }
extern "C" __declspec(dllexport) bool InitializePlugin(DWORD) { return true; }
extern "C" __declspec(dllexport) void UninitializePlugin() {}
extern "C" __declspec(dllexport) COMMON_PLUGIN_TABLE* GetCommonPluginTable() { return &table; }
extern "C" __declspec(dllexport) void RegisterPlugin(HOST_APP_TABLE* host) {
  g.edit = host->create_edit_handle();
  WNDCLASSEXW klass{}; klass.cbSize = sizeof(klass); klass.lpfnWndProc = wndproc;
  klass.hInstance = GetModuleHandleW(nullptr); klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  klass.lpszClassName = kWindowClass;
  if (!RegisterClassExW(&klass)) return;
  HWND window = CreateWindowExW(0, kWindowClass, kWindowTitle, WS_POPUP,
                              CW_USEDEFAULT, CW_USEDEFAULT, 400, 500, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
  if (!window) return;
  host->register_window_client(kWindowTitle, window);
  host->register_project_load_handler(project_load);
  host->register_project_save_handler(project_save);
  host->register_object_menu_param(L"SceneShelf\\選択オブジェクトからシーン作成", nullptr,
                                   create_scene_from_object_menu);
}
