#include "map/Tile.h"

Tile::Tile()
    : m_type(TileType::Floor), m_row(0), m_col(0),
      m_texTileX(-1), m_texTileY(-1) {}

Tile::Tile(TileType type, int row, int col)
    : m_type(type), m_row(row), m_col(col),
      m_texTileX(-1), m_texTileY(-1) {}

TileType Tile::getType() const { return m_type; }

bool Tile::isWalkable() const {
    return m_type == TileType::Floor || m_type == TileType::Spawn;
}

void Tile::setType(TileType type) { m_type = type; }

void Tile::setTextureRect(int tileX, int tileY) {
    m_texTileX = tileX;
    m_texTileY = tileY;
}

int Tile::getTextureTileX() const { return m_texTileX; }
int Tile::getTextureTileY() const { return m_texTileY; }
