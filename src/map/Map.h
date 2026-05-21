#pragma once

#include <vector>
#include <string>
#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include "map/Tile.h"

// ============================================================
// 碰撞区域结构体
// 从 Tiled 编辑器的 collision 对象层解析得到
// ============================================================
struct CollisionRect {
    float x, y, width, height; // 世界坐标位置和尺寸
};

// ============================================================
// 多边形碰撞体结构体
// 从 Tiled 编辑器的 <polygon> 标签解析得到
// ============================================================
struct CollisionPolygon {
    std::vector<sf::Vector2f> points; // 世界坐标顶点列表（按顺序连接）
};

// ============================================================
// Tileset 信息结构体
// 存储单个 tileset 的元数据
// ============================================================
struct TilesetInfo {
    int         firstGid;   // 在 tmx 文件中的起始 gid（全局 tile ID）
    sf::Texture texture;    // tileset 纹理
    bool        loaded;     // 纹理是否已加载
    int         columns;    // 每行 tile 数量
    int         spacing;    // tile 之间的间距（像素）
    int         margin;     // tileset 外边距（像素）
    int         tileCount;  // tile 总数
};

// ============================================================
// 地图类
// 负责加载和管理 Tiled 格式的地图数据
// 
// 功能特点：
//   - 支持从 Tiled .tmx 文件加载地图
//   - 支持多层 tile 渲染
//   - 支持矩形和多边形碰撞检测
//   - 提供出生点和出口点信息
// ============================================================
class Map {
public:
    /**
     * @brief 默认构造函数
     */
    Map();
    
    /**
     * @brief 构造函数（手动创建地图）
     * @param name 地图名称
     * @param rows 行数
     * @param cols 列数
     */
    Map(const std::string& name, int rows, int cols);

    /**
     * @brief 从 Tiled .tmx 文件加载地图
     * @param tmxPath tmx 文件路径
     * @return true 表示加载成功
     * @note 支持 XML 格式的 .tmx 文件，自动解析图层、tileset 和碰撞信息
     */
    bool loadFromTiled(const std::string& tmxPath);

    /**
     * @brief 获取指定位置的 tile
     * @param row 行索引
     * @param col 列索引
     * @return Tile 引用
     */
    Tile& getTile(int row, int col);
    
    /**
     * @brief 检查指定位置是否可通行
     * @param row 行索引
     * @param col 列索引
     * @return true 表示可通行
     */
    bool isWalkable(int row, int col) const;

    /**
     * @brief 获取地图行数
     * @return 行数
     */
    int getRows() const;
    
    /**
     * @brief 获取地图列数
     * @return 列数
     */
    int getCols() const;
    
    /**
     * @brief 获取地图名称
     * @return 地图名称
     */
    const std::string& getName() const;

    /**
     * @brief 获取玩家出生点
     * @return 出生点坐标（世界坐标）
     */
    sf::Vector2f getSpawnPoint() const;
    
    /**
     * @brief 获取地图出口点
     * @return 出口点坐标（世界坐标）
     */
    sf::Vector2f getExitPoint() const;

    /**
     * @brief 点碰撞检测
     * @param x X 坐标（世界坐标）
     * @param y Y 坐标（世界坐标）
     * @return true 表示该点与碰撞区域重叠
     * @note 支持矩形和多边形碰撞体检测
     */
    bool isColliding(float x, float y) const;

    /**
     * @brief 矩形碰撞检测
     * @param rx 矩形左上角 X 坐标（世界坐标）
     * @param ry 矩形左上角 Y 坐标（世界坐标）
     * @param rw 矩形宽度
     * @param rh 矩形高度
     * @return true 表示矩形与碰撞区域重叠
     * @note 支持矩形和多边形碰撞体检测
     */
    bool isRectColliding(float rx, float ry, float rw, float rh) const;

    /**
     * @brief 获取所有矩形碰撞区域
     * @return 碰撞区域列表
     */
    const std::vector<CollisionRect>& getCollisionRects() const;

    /**
     * @brief 渲染地图底层
     * @param window 渲染窗口
     * @param tileSize 渲染 tile 尺寸（像素）
     * @note 渲染所有普通 tile 图层和对象
     */
    void render(sf::RenderWindow& window, int tileSize) const;

    /**
     * @brief 渲染覆盖图层
     * @param window 渲染窗口
     * @param tileSize 渲染 tile 尺寸（像素）
     * @note 渲染名称为 "overlay" 的 tile 图层，显示在角色之上
     */
    void renderOverlays(sf::RenderWindow& window, int tileSize) const;

    /**
     * @brief 获取 tile 宽度
     * @return tile 宽度（像素）
     */
    int getTileWidth() const { return m_tileWidth; }
    
    /**
     * @brief 获取 tile 高度
     * @return tile 高度（像素）
     */
    int getTileHeight() const { return m_tileHeight; }

private:
    // ============================================================
    // 地图基本信息
    // ============================================================
    
    std::string m_name;      // 地图名称
    int         m_rows;      // 行数
    int         m_cols;      // 列数
    int         m_tileWidth; // 单个 tile 宽度（像素）
    int         m_tileHeight;// 单个 tile 高度（像素）

    // ============================================================
    // 图层数据
    // ============================================================
    
    // 多层 tile 数据：[layer][row][col] -> tile gid
    std::vector<std::vector<std::vector<int>>> m_layerData;
    
    // 图层名称列表（与 m_layerData 索引对应）
    std::vector<std::string> m_layerNames;

    // ============================================================
    // Tileset 数据
    // ============================================================
    
    std::vector<TilesetInfo> m_tilesets; // 所有 tileset 信息

    // ============================================================
    // 碰撞数据
    // ============================================================
    
    std::vector<CollisionRect>    m_collisions;  // 矩形碰撞区域
    std::vector<CollisionPolygon> m_polygons;    // 多边形碰撞体

    // ============================================================
    // 特殊点
    // ============================================================
    
    sf::Vector2f m_spawnPoint; // 玩家出生点
    sf::Vector2f m_exitPoint;  // 地图出口点

    // ============================================================
    // 内部辅助结构
    // ============================================================
    
    /**
     * @brief Tile 查找结果结构体
     */
    struct TileLookup {
        const TilesetInfo* tileset;  // 对应的 tileset
        sf::IntRect        texRect;  // 纹理区域
    };
    
    /**
     * @brief 根据 gid 查找对应的 tileset 和纹理区域
     * @param gid tile 全局 ID
     * @return TileLookup 结构
     */
    TileLookup lookupTile(int gid) const;
};
