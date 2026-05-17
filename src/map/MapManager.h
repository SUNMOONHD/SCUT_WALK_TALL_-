#pragma once

#include <vector>
#include <memory>
#include <string>
#include "map/Map.h"

class MapManager {
public:
    MapManager();
    ~MapManager() = default;

    /// 加载 assets/maps/ 下所有 .tmx 地图
    void loadMaps();

    Map& getCurrentMap();
    void switchMap(const std::string& mapName);

    int getMapCount() const;

private:
    std::vector<std::unique_ptr<Map>> m_maps;
    int m_currentIndex;
};
