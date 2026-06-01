#pragma once

#include <SFML/Graphics/Texture.hpp>

enum class TileType {
    Floor,
    Wall,
    Obstacle,
    Spawn,
    Exit
};

class Tile {
public:
    Tile();
    Tile(TileType type, int row, int col);

    TileType getType() const;
    bool isWalkable() const;
    void setType(TileType type);

    // 纹理信息（每个 tile 对应 tileset 上的一小块）
    void setTextureRect(int tileX, int tileY); // tileset 内的 tile 索引坐标
    int getTextureTileX() const;
    int getTextureTileY() const;

private:
    TileType m_type;
    int m_row;
    int m_col;
    int m_texTileX; // 纹理在 tileset 的列索引
    int m_texTileY; // 纹理在 tileset 的行索引
};
