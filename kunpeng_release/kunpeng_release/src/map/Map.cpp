#include "map/Map.h"
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <pugixml.hpp>
#include <cstdio>
#include <sstream>
#include <fstream>
#include <cmath>
#include <algorithm>

Map::Map()
    : m_name(""), m_rows(0), m_cols(0),
      m_tileWidth(16), m_tileHeight(16),
      m_spawnPoint(0, 0), m_exitPoint(0, 0) {}

Map::Map(const std::string& name, int rows, int cols)
    : m_name(name), m_rows(rows), m_cols(cols),
      m_tileWidth(16), m_tileHeight(16),
      m_spawnPoint(0, 0), m_exitPoint(0, 0) {}

bool Map::loadFromTiled(const std::string& tmxPath) {
    pugi::xml_document doc;
    if (!doc.load_file(tmxPath.c_str())) {
        return false;
    }

    pugi::xml_node mapNode = doc.child("map");
    if (!mapNode) return false;

    // Tiled 标准: width=列数, height=行数
    m_cols = mapNode.attribute("width").as_int();
    m_rows = mapNode.attribute("height").as_int();
    m_tileWidth = mapNode.attribute("tilewidth").as_int(16);
    m_tileHeight = mapNode.attribute("tileheight").as_int(16);

    // 提取 tmx 所在目录
    std::string tmxDir;
    size_t lastSlash = tmxPath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        tmxDir = tmxPath.substr(0, lastSlash + 1);
    }

    // ── 加载所有 tileset ──
    m_tilesets.clear();
    for (pugi::xml_node tsNode = mapNode.child("tileset");
         tsNode;
         tsNode = tsNode.next_sibling("tileset"))
    {
        TilesetInfo info;
        info.firstGid  = tsNode.attribute("firstgid").as_int(1);
        info.columns   = tsNode.attribute("columns").as_int(0);
        info.spacing   = tsNode.attribute("spacing").as_int(0);
        info.margin    = tsNode.attribute("margin").as_int(0);
        info.tileCount = tsNode.attribute("tilecount").as_int(0);
        info.loaded    = false;

        pugi::xml_node imgNode = tsNode.child("image");
        if (imgNode) {
            std::string imgSource = imgNode.attribute("source").as_string();
            std::string imgPath = tmxDir + imgSource;
            info.loaded = info.texture.loadFromFile(imgPath);
        }

        m_tilesets.push_back(std::move(info));
    }

    // ── 解析所有图层 ──
    m_layerData.clear();
    m_layerNames.clear();
    for (pugi::xml_node layerNode = mapNode.child("layer");
         layerNode;
         layerNode = layerNode.next_sibling("layer"))
    {
        std::string layerName = layerNode.attribute("name").as_string("");
        m_layerNames.push_back(layerName);
        std::vector<std::vector<int>> layer;
        layer.resize(m_rows, std::vector<int>(m_cols, 0));

        pugi::xml_node dataNode = layerNode.child("data");
        if (!dataNode) continue;

        std::string encoding = dataNode.attribute("encoding").as_string();

        if (encoding == "csv") {
            // CSV 格式
            std::string csv = dataNode.text().as_string();
            std::stringstream ss(csv);
            std::string cell;
            int row = 0, col = 0;
            while (std::getline(ss, cell, ',') && row < m_rows) {
                cell.erase(std::remove_if(cell.begin(), cell.end(), ::isspace), cell.end());
                if (!cell.empty()) {
                    layer[row][col] = std::stoi(cell);
                    col++;
                    if (col >= m_cols) {
                        col = 0;
                        row++;
                    }
                }
            }
        } else {
            // XML 格式（默认），逐个 <tile gid="...">
            int row = 0, col = 0;
            for (pugi::xml_node tileNode = dataNode.child("tile");
                 tileNode && row < m_rows;
                 tileNode = tileNode.next_sibling("tile"))
            {
                layer[row][col] = tileNode.attribute("gid").as_int(0);
                col++;
                if (col >= m_cols) {
                    col = 0;
                    row++;
                }
            }
        }

        m_layerData.push_back(std::move(layer));
    }

    // ── 碰撞箱：只从对象层解析，不从 tile 图层生成 ──
    m_collisions.clear();
    m_polygons.clear();

    for (pugi::xml_node objGroup = mapNode.child("objectgroup");
         objGroup;
         objGroup = objGroup.next_sibling("objectgroup"))
    {
        for (pugi::xml_node obj = objGroup.child("object");
             obj;
             obj = obj.next_sibling("object"))
        {
            std::string objType = obj.attribute("type").as_string("");
            std::string objName = obj.attribute("name").as_string("");

            float ox = obj.attribute("x").as_float(0);
            float oy = obj.attribute("y").as_float(0);
            float ow = obj.attribute("width").as_float(0);
            float oh = obj.attribute("height").as_float(0);

            std::string customType = "";
            bool isCollidable = false;
            pugi::xml_node props = obj.child("properties");
            if (props) {
                for (pugi::xml_node prop = props.child("property");
                     prop;
                     prop = prop.next_sibling("property"))
                {
                    std::string pName = prop.attribute("name").as_string();
                    if (pName == "type") {
                        customType = prop.attribute("value").as_string("");
                    }
                    if (pName == "collidable" && prop.attribute("value").as_bool(false)) {
                        isCollidable = true;
                    }
                }
            }

            std::string effectiveType = customType.empty() ? objType : customType;

            if (effectiveType == "collision" || isCollidable) {
                pugi::xml_node polyNode = obj.child("polygon");
                if (polyNode) {
                    std::string points = polyNode.attribute("points").as_string("");
                    CollisionPolygon poly;
                    std::stringstream pss(points);
                    std::string ptStr;
                    while (std::getline(pss, ptStr, ' ')) {
                        if (ptStr.empty()) continue;
                        float px = 0, py = 0;
                        if (std::sscanf(ptStr.c_str(), "%f,%f", &px, &py) == 2) {
                            poly.points.push_back({ox + px, oy + py});
                        }
                    }
                    if (poly.points.size() >= 3) {
                        m_polygons.push_back(std::move(poly));
                    }
                } else if (ow > 0 && oh > 0) {
                    m_collisions.push_back({ox, oy, ow, oh});
                }
            } else if (effectiveType == "spawn") {
                m_spawnPoint = sf::Vector2f(ox, oy);
            } else if (effectiveType == "exit") {
                m_exitPoint = sf::Vector2f(ox, oy);
            }
        }
    }

    // 默认出生点（如果没有在 Tiled 中标记）：地图中央
    if (m_spawnPoint == sf::Vector2f(0, 0)) {
        m_spawnPoint = sf::Vector2f(
            m_tileWidth * (m_cols * 0.5f),
            m_tileHeight * (m_rows * 0.5f)
        );
    }
    if (m_exitPoint == sf::Vector2f(0, 0)) {
        m_exitPoint = sf::Vector2f(
            m_tileWidth * (m_cols - 3.0f),
            m_tileHeight * (m_rows - 3.0f)
        );
    }

    return true;
}

Tile& Map::getTile(int row, int col) {
    static Tile dummy;
    if (row < 0 || row >= m_rows || col < 0 || col >= m_cols) return dummy;

    static Tile tile;
    return tile;
}

bool Map::isWalkable(int row, int col) const {
    return row >= 0 && row < m_rows && col >= 0 && col < m_cols;
}

int Map::getRows() const { return m_rows; }
int Map::getCols() const { return m_cols; }
const std::string& Map::getName() const { return m_name; }

sf::Vector2f Map::getSpawnPoint() const { return m_spawnPoint; }
sf::Vector2f Map::getExitPoint() const { return m_exitPoint; }

bool Map::isColliding(float x, float y) const {
    // 矩形碰撞检测
    for (const auto& rect : m_collisions) {
        if (x >= rect.x && x < rect.x + rect.width &&
            y >= rect.y && y < rect.y + rect.height) {
            return true;
        }
    }
    // 多边形碰撞检测（射线法）
    for (const auto& poly : m_polygons) {
        const auto& pts = poly.points;
        int n = static_cast<int>(pts.size());
        bool inside = false;
        for (int i = 0, j = n - 1; i < n; j = i++) {
            float xi = pts[i].x, yi = pts[i].y;
            float xj = pts[j].x, yj = pts[j].y;
            if (((yi > y) != (yj > y)) &&
                (x < (xj - xi) * (y - yi) / (yj - yi) + xi)) {
                inside = !inside;
            }
        }
        if (inside) return true;
    }
    return false;
}

bool Map::isRectColliding(float rx, float ry, float rw, float rh) const {
    // 矩形-矩形 AABB 检测
    for (const auto& rect : m_collisions) {
        if (rx < rect.x + rect.width && rx + rw > rect.x &&
            ry < rect.y + rect.height && ry + rh > rect.y) {
            return true;
        }
    }
    // 矩形-多边形碰撞检测
    for (const auto& poly : m_polygons) {
        const auto& pts = poly.points;
        int n = static_cast<int>(pts.size());
        if (n < 3) continue;

        // AABB 快速排除
        float pMinX = pts[0].x, pMaxX = pts[0].x;
        float pMinY = pts[0].y, pMaxY = pts[0].y;
        for (int i = 1; i < n; ++i) {
            pMinX = std::min(pMinX, pts[i].x);
            pMaxX = std::max(pMaxX, pts[i].x);
            pMinY = std::min(pMinY, pts[i].y);
            pMaxY = std::max(pMaxY, pts[i].y);
        }
        if (rx >= pMaxX || rx + rw <= pMinX || ry >= pMaxY || ry + rh <= pMinY)
            continue;

        // 检查矩形4个顶点是否在多边形内（射线法）
        if (isColliding(rx, ry) || isColliding(rx + rw, ry) ||
            isColliding(rx, ry + rh) || isColliding(rx + rw, ry + rh))
            return true;

        // 检查多边形顶点是否在矩形内
        for (int i = 0; i < n; ++i) {
            if (pts[i].x >= rx && pts[i].x <= rx + rw &&
                pts[i].y >= ry && pts[i].y <= ry + rh)
                return true;
        }

        // 边相交检测（检查矩形边是否与多边形边相交）
        auto edgesIntersect = [](float ax1, float ay1, float ax2, float ay2,
                                  float bx1, float by1, float bx2, float by2) -> bool {
            float d1x = ax2 - ax1, d1y = ay2 - ay1;
            float d2x = bx2 - bx1, d2y = by2 - by1;
            float cross = d1x * d2y - d1y * d2x;
            if (std::abs(cross) < 0.0001f) return false;
            float t = ((bx1 - ax1) * d2y - (by1 - ay1) * d2x) / cross;
            float u = ((bx1 - ax1) * d1y - (by1 - ay1) * d1x) / cross;
            return t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f;
        };

        // 矩形的4条边
        float rEdges[4][4] = {
            {rx, ry, rx + rw, ry},
            {rx + rw, ry, rx + rw, ry + rh},
            {rx + rw, ry + rh, rx, ry + rh},
            {rx, ry + rh, rx, ry}
        };
        for (int re = 0; re < 4; ++re) {
            for (int pe = 0; pe < n; ++pe) {
                int pne = (pe + 1) % n;
                if (edgesIntersect(rEdges[re][0], rEdges[re][1], rEdges[re][2], rEdges[re][3],
                                   pts[pe].x, pts[pe].y, pts[pne].x, pts[pne].y))
                    return true;
            }
        }
    }
    return false;
}

const std::vector<CollisionRect>& Map::getCollisionRects() const {
    return m_collisions;
}

// ── 根据 gid 查找对应的 tileset 和纹理区域 ──
Map::TileLookup Map::lookupTile(int gid) const {
    TileLookup result = { nullptr, sf::IntRect({0, 0}, {0, 0}) };

    if (gid <= 0) return result;

    // 找到 gid 所属的 tileset（倒序查找，支持 tileset 重叠覆盖）
    const TilesetInfo* found = nullptr;
    for (int i = static_cast<int>(m_tilesets.size()) - 1; i >= 0; --i) {
        if (m_tilesets[i].loaded && gid >= m_tilesets[i].firstGid) {
            found = &m_tilesets[i];
            break;
        }
    }

    if (!found || found->columns <= 0) return result;

    result.tileset = found;

    int localId = gid - found->firstGid; // 转为 0-based
    int col = localId % found->columns;
    int row = localId / found->columns;

    int x = found->margin + col * (m_tileWidth + found->spacing);
    int y = found->margin + row * (m_tileHeight + found->spacing);

    result.texRect = sf::IntRect({x, y}, {m_tileWidth, m_tileHeight});
    return result;
}

void Map::render(sf::RenderWindow& window, int tileSize) const {
    if (m_tilesets.empty() || m_layerData.empty()) return;

    int effectiveTileSize = (tileSize > 0) ? tileSize : m_tileWidth;
    float scale = static_cast<float>(effectiveTileSize) / m_tileWidth;

    const sf::Texture* lastTex = nullptr;

    for (size_t li = 0; li < m_layerData.size(); ++li) {
        // 跳过 overlay 图层，由 renderOverlays 单独渲染
        if (li < m_layerNames.size() && m_layerNames[li] == "overlay")
            continue;

        const auto& layer = m_layerData[li];
        for (int r = 0; r < m_rows; ++r) {
            for (int c = 0; c < m_cols; ++c) {
                int gid = layer[r][c];
                if (gid <= 0) continue;

                TileLookup lookup = lookupTile(gid);
                if (!lookup.tileset || lookup.texRect.size.x == 0 || lookup.texRect.size.y == 0)
                    continue;

                if (&lookup.tileset->texture != lastTex) {
                    lastTex = &lookup.tileset->texture;
                }

                sf::Sprite sprite(*lastTex);
                sprite.setScale({scale, scale});
                sprite.setTextureRect(lookup.texRect);
                sprite.setPosition({
                    static_cast<float>(c * effectiveTileSize),
                    static_cast<float>(r * effectiveTileSize)
                });
                window.draw(sprite);
            }
        }
    }
}

void Map::renderOverlays(sf::RenderWindow& window, int tileSize) const {
    if (m_tilesets.empty() || m_layerData.empty()) return;

    int effectiveTileSize = (tileSize > 0) ? tileSize : m_tileWidth;
    float scale = static_cast<float>(effectiveTileSize) / m_tileWidth;

    // 找到名为 "overlay" 的图层
    int overlayIdx = -1;
    for (size_t i = 0; i < m_layerNames.size(); ++i) {
        if (m_layerNames[i] == "overlay") {
            overlayIdx = static_cast<int>(i);
            break;
        }
    }
    if (overlayIdx < 0 || overlayIdx >= static_cast<int>(m_layerData.size()))
        return;

    const auto& layer = m_layerData[overlayIdx];
    const sf::Texture* lastTex = nullptr;

    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_cols; ++c) {
            int gid = layer[r][c];
            if (gid <= 0) continue;

            TileLookup lookup = lookupTile(gid);
            if (!lookup.tileset || lookup.texRect.size.x == 0 || lookup.texRect.size.y == 0)
                continue;

            if (&lookup.tileset->texture != lastTex) {
                lastTex = &lookup.tileset->texture;
            }

            sf::Sprite sprite(*lastTex);
            sprite.setScale({scale, scale});
            sprite.setTextureRect(lookup.texRect);
            sprite.setPosition({
                static_cast<float>(c * effectiveTileSize),
                static_cast<float>(r * effectiveTileSize)
            });
            window.draw(sprite);
        }
    }
}
