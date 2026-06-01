#pragma once

#include "metro_graph.h"

#include <QtNodes/AbstractGraphModel>
#include <QtNodes/BasicGraphicsScene>
#include <QtNodes/Definitions>
#include <QtNodes/GraphicsView>
#include <QtNodes/internal/AbstractNodePainter.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QToolBar>
#include <QVBoxLayout>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using ConnectionId = QtNodes::ConnectionId;
using NodeId = QtNodes::NodeId;
using NodeRole = QtNodes::NodeRole;
using PortIndex = QtNodes::PortIndex;
using PortRole = QtNodes::PortRole;
using PortType = QtNodes::PortType;
using QtNodes::InvalidNodeId;

class StationNodePainter : public QtNodes::AbstractNodePainter
{
public:
    void paint(QPainter *painter, QtNodes::NodeGraphicsObject &ngo) const override;
};

class MetroGraphModel : public QtNodes::AbstractGraphModel
{
    Q_OBJECT
public:
    explicit MetroGraphModel(MetroGraph &graph);

    void loadFromMetroGraph();
    void saveToMetroGraph();

    std::unordered_set<NodeId> allNodeIds() const override;
    std::unordered_set<ConnectionId> allConnectionIds(NodeId nodeId) const override;
    std::unordered_set<ConnectionId> connections(NodeId nodeId,
                                                 PortType portType,
                                                 PortIndex portIndex) const override;
    bool connectionExists(ConnectionId const connectionId) const override;
    NodeId addNode(QString const nodeType = QString()) override;
    bool connectionPossible(ConnectionId const connectionId) const override;
    void addConnection(ConnectionId const connectionId) override;
    bool nodeExists(NodeId const nodeId) const override;
    QVariant nodeData(NodeId nodeId, NodeRole role) const override;
    bool setNodeData(NodeId nodeId, NodeRole role, QVariant value) override;
    QVariant portData(NodeId nodeId,
                      PortType portType,
                      PortIndex portIndex,
                      PortRole role) const override;
    bool setPortData(NodeId nodeId,
                     PortType portType,
                     PortIndex portIndex,
                     QVariant const &value,
                     PortRole role = PortRole::Data) override;
    bool deleteConnection(ConnectionId const connectionId) override;
    bool deleteNode(NodeId const nodeId) override;
    QJsonObject saveNode(NodeId const) const override;
    void loadNode(QJsonObject const &nodeJson) override;
    NodeId newNodeId() override { return _nextNodeId++; }

    NodeId nodeIdForName(const std::string &name) const;
    std::string nodeNameForId(NodeId id) const;

    struct NodeInfo {
        std::string metroId;
        std::string type = "platform";
        int floor = 0;
        double capacity = 200.0;
        double width = 3.0;
        QString icon = "node_platform.svg";
        QPointF pos;
    };

    struct EdgeInfo {
        double length = 10.0;
        double width = 2.0;
        double capacity = 100.0;
        double transferTime = 1.0;
        int lineIndex = 1;
        bool bidirectional = true;
    };

    const NodeInfo *nodeInfo(NodeId id) const;
    EdgeInfo *edgeInfo(ConnectionId cid);
    const std::unordered_map<NodeId, NodeInfo> &nodeMap() const { return _nodeMap; }
    const std::unordered_set<ConnectionId> &allConnections() const { return _connections; }

private:
    MetroGraph &_graph;
    NodeId _nextNodeId = 0;
    std::unordered_map<NodeId, NodeInfo> _nodeMap;
    std::unordered_map<std::string, NodeId> _metroIdToNodeId;
    std::unordered_set<ConnectionId> _connections;
    std::unordered_map<ConnectionId, EdgeInfo> _edgeInfoMap;
};

class StationEditorWidget : public QDialog
{
    Q_OBJECT
public:
    explicit StationEditorWidget(MetroGraph &graph, QWidget *parent = nullptr);
    ~StationEditorWidget();

    MetroGraph &graph() const { return _graph; }

private slots:
    void onSave();
    void onAddNode();
    void onDeleteSelected();
    void onSelectionChanged();
    void onPresetSmall();
    void onPresetLarge();
    void onPresetHuge();
    void onNodePropertyChanged();
    void onEdgePropertyChanged();
    void onAutoLayout();

private:
    struct PresetNodeInfo {
        double x, y;
        QString type, icon;
        int floor = 0;
        double capacity = 200.0;
        double width = 3.0;
    };
    void buildUi();
    void buildRightPanel(QWidget *parent);
    void buildPresetSection(QVBoxLayout *layout);
    void buildNodePropsSection(QVBoxLayout *layout);
    void buildEdgePropsSection(QVBoxLayout *layout);
    void applyNodeStyle();
    void refreshNodeProps(NodeId nodeId);
    void refreshEdgeProps(ConnectionId cid);
    void clearNodeProps();
    void clearEdgeProps();
    void loadPreset(const std::vector<std::tuple<double, double, QString, QString>> &nodes,
                    const std::vector<std::tuple<int, int>> &edges);
    void loadPreset(const std::vector<PresetNodeInfo> &nodes,
                    const std::vector<std::tuple<int, int>> &edges);
    QStringList availableIcons() const;

    MetroGraph &_graph;
    MetroGraphModel *_model;
    QtNodes::BasicGraphicsScene *_scene;
    QtNodes::GraphicsView *_view;
    QToolBar *_toolbar;

    QWidget *_rightPanel;
    QGroupBox *_nodePropsGroup;
    QGroupBox *_edgePropsGroup;
    QLabel *_nodePropsPlaceholder;
    QLabel *_edgePropsPlaceholder;

    QLineEdit *_nodeNameEdit;
    QComboBox *_nodeTypeCombo;
    QSpinBox *_nodeFloorSpin;
    QDoubleSpinBox *_nodeCapacitySpin;
    QDoubleSpinBox *_nodeWidthSpin;
    QDoubleSpinBox *_nodeXSpin;
    QDoubleSpinBox *_nodeYSpin;
    QComboBox *_nodeIconCombo;

    QDoubleSpinBox *_edgeLengthSpin;
    QDoubleSpinBox *_edgeWidthSpin;
    QDoubleSpinBox *_edgeCapacitySpin;
    QDoubleSpinBox *_edgeTransferSpin;
    QSpinBox *_edgeLineSpin;
    QCheckBox *_edgeBidirCheck;

    NodeId _currentNodeId = InvalidNodeId;
    ConnectionId _currentEdgeId{InvalidNodeId, 0, InvalidNodeId, 0};
    bool _updatingProps = false;
};