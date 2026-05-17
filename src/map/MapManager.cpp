#include "map/MapManager.h"
#include <filesystem>

MapManager::MapManager() : m_currentIndex(0) {}

void MapManager::loadMaps() {
    m_maps.clear();
    m_currentIndex = 0;

    const std::string mapDir = "assets/maps/";

    // 按文件名排序加载所有 .tmx 文件
    std::vector<std::string> tmxFiles;
    for (const auto& entry : std::filesystem::directory_iterator(mapDir)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            // 统一转小写比较
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".tmx") {
                tmxFiles.push_back(entry.path().string());
            }
        }
    }
    std::sort(tmxFiles.begin(), tmxFiles.end());

    for (const auto& path : tmxFiles) {
        auto map = std::make_unique<Map>();
        if (map->loadFromTiled(path)) {
            // 从文件名提取地图名（去掉目录和扩展名）
            std::string filename = std::filesystem::path(path).stem().string();
            m_maps.push_back(std::move(map));
        }
    }
}

Map& MapManager::getCurrentMap() {
    if (m_maps.empty()) {
        // 兜底：返回一个空地图
        static Map fallback;
        return fallback;
    }
    return *m_maps[m_currentIndex];
}

void MapManager::switchMap(const std::string& mapName) {
    for (int i = 0; i < static_cast<int>(m_maps.size()); ++i) {
        if (m_maps[i]->getName() == mapName) {
            m_currentIndex = i;
            return;
        }
    }
}

int MapManager::getMapCount() const {
    return static_cast<int>(m_maps.size());
}
