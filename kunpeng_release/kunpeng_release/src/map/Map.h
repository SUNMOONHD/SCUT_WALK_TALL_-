#pragma once

#include <vector>
#include <string>
#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include "map/Tile.h"

// 碰撞区域（从 Tiled 对象层 collision 标记解析）
struct CollisionRect {
    float x, y, width, height;
};

// 多边形碰撞体（从 Tiled 对象层 <polygon> 解析）
struct CollisionPolygon {
    std::vector<sf::Vector2f> points; // 世界坐标顶点列表
};

// 单个 Tileset 的信息
struct TilesetInfo {
    int firstGid;            // 在 tmx 中的起始 gid
    sf::Texture texture;     // 纹理
    bool loaded;
    int columns;             // 每行 tile 数
    int spacing;             // tile 间距
    int margin;              // 外边距
    int tileCount;           // tile 总数
};

class Map {
public:
    Map();
    Map(const std::string& name, int rows, int cols);

    /// 从 Tiled .tmx 文件加载地图
    bool loadFromTiled(const std::string& tmxPath);

    Tile& getTile(int row, int col);
    bool isWalkable(int row, int col) const;

    int getRows() const;
    int getCols() const;
    const std::string& getName() const;

    sf::Vector2f getSpawnPoint() const;
    sf::Vector2f getExitPoint() const;

    /// 碰撞检测：检查点是否在某个碰撞区域内（含多边形）
    bool isColliding(float x, float y) const;

    /// 碰撞检测：矩形与碰撞区域是否有重叠（含多边形）
    bool isRectColliding(float rx, float ry, float rw, float rh) const;

    const std::vector<CollisionRect>& getCollisionRects() const;

    /// 渲染地图底层（tile 图层 + 普通对象）
    void render(sf::RenderWindow& window, int tileSize) const;

    /// 渲染覆盖图层（名称为 "overlay" 的 tile 图层，画在角色之上）
    void renderOverlays(sf::RenderWindow& window, int tileSize) const;

    int getTileWidth() const { return m_tileWidth; }
    int getTileHeight() const { return m_tileHeight; }

private:
    std::string m_name;
    int m_rows;
    int m_cols;
    int m_tileWidth;
    int m_tileHeight;

    // 多个图层：每个图层是 rows×cols 的 tile ID 数组（0 表示空）
    std::vector<std::vector<std::vector<int>>> m_layerData; // [layer][row][col]

    // 图层名称（与 m_layerData 一一对应）
    std::vector<std::string> m_layerNames;

    // 多个 Tileset
    std::vector<TilesetInfo> m_tilesets;

    // 碰撞区域列表
    std::vector<CollisionRect> m_collisions;

    // 多边形碰撞体列表
    std::vector<CollisionPolygon> m_polygons;

    // 出生点和出口
    sf::Vector2f m_spawnPoint;
    sf::Vector2f m_exitPoint;

    // 根据 gid 找到对应的 tileset 和纹理区域
    struct TileLookup {
        const TilesetInfo* tileset;
        sf::IntRect texRect;
    };
    TileLookup lookupTile(int gid) const;
};
