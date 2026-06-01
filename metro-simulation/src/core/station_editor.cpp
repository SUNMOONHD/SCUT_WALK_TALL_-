#include "station_editor.h"

#include <QtNodes/internal/ConnectionGraphicsObject.hpp>
#include <QtNodes/ConnectionStyle>
#include <QtNodes/internal/NodeGraphicsObject.hpp>
#include <QtNodes/NodeStyle>
#include <QtNodes/StyleCollection>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QQueue>
#include <QScreen>
#include <QSet>
#include <QSplitter>
#include <memory>
#include <set>

static constexpr double kCoordScale = 10.0;

static const QStringList kNodeTypes = {
    "entrance", "exit", "platform", "corridor",
    "stairs", "escalator", "gate", "security",
    "ticket", "hall", "waiting"
};

static const QStringList kNodeTypeLabels = {
    QStringLiteral("入口"), QStringLiteral("出口"), QStringLiteral("站台"),
    QStringLiteral("通道"), QStringLiteral("楼梯"), QStringLiteral("扶梯"),
    QStringLiteral("闸机"), QStringLiteral("安检"), QStringLiteral("售票"),
    QStringLiteral("大厅"), QStringLiteral("候车区")
};

static QString chineseLabelForType(const QString &type)
{
    for (int i = 0; i < kNodeTypes.size(); ++i) {
        if (kNodeTypes[i] == type)
            return kNodeTypeLabels[i];
    }
    return QStringLiteral("节点");
}

static QString resolveIconPath(const QString &iconName)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).absoluteFilePath("resources/icons/" + iconName),
        QDir(appDir).absoluteFilePath("../resources/icons/" + iconName),
        QDir(appDir).absoluteFilePath("../../resources/icons/" + iconName),
        QDir::current().absoluteFilePath("resources/icons/" + iconName),
    };
    for (const auto &c : candidates) {
        if (QFileInfo::exists(c))
            return c;
    }
    return candidates.front();
}

void StationNodePainter::paint(QPainter *painter, QtNodes::NodeGraphicsObject &ngo) const
{
    QtNodes::AbstractGraphModel &model = ngo.graphModel();
    QtNodes::NodeId const nodeId = ngo.nodeId();
    QtNodes::AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();

    QSize size = geometry.size(nodeId);

    QJsonDocument json = QJsonDocument::fromVariant(model.nodeData(nodeId, QtNodes::NodeRole::Style));
    QtNodes::NodeStyle nodeStyle(json.object());

    QColor borderColor = ngo.isSelected() ? nodeStyle.SelectedBoundaryColor
                                          : nodeStyle.NormalBoundaryColor;

    if (ngo.nodeState().hovered()) {
        QPen p(borderColor, nodeStyle.HoveredPenWidth);
        painter->setPen(p);
    } else {
        QPen p(borderColor, nodeStyle.PenWidth);
        painter->setPen(p);
    }

    QLinearGradient gradient(QPointF(0.0, 0.0), QPointF(2.0, size.height()));
    gradient.setColorAt(0.0, nodeStyle.GradientColor0);
    gradient.setColorAt(0.10, nodeStyle.GradientColor1);
    gradient.setColorAt(0.90, nodeStyle.GradientColor2);
    gradient.setColorAt(1.0, nodeStyle.GradientColor3);
    painter->setBrush(gradient);

    double const radius = 3.0;
    QRectF boundary(0, 0, size.width(), size.height());
    painter->drawRoundedRect(boundary, radius, radius);

    QJsonObject internal = model.nodeData(nodeId, QtNodes::NodeRole::InternalData).toJsonObject();
    QString iconName = internal["icon"].toString();
    QString typeStr = internal["type"].toString();

    if (!iconName.isEmpty()) {
        static QHash<QString, QPixmap> iconCache;
        auto it = iconCache.find(iconName);
        if (it == iconCache.end()) {
            QString iconPath = resolveIconPath(iconName);
            QPixmap pixmap(iconPath);
            if (!pixmap.isNull()) {
                pixmap = pixmap.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            it = iconCache.insert(iconName, pixmap);
        }
        if (!it->isNull()) {
            double iconX = (size.width() - it->width()) / 2.0;
            double iconY = 4.0;
            painter->drawPixmap(QPointF(iconX, iconY), *it);
        }
    }

    for (int i = 0; i < kNodeTypes.size(); ++i) {
        if (kNodeTypes[i] == typeStr) {
            typeStr = kNodeTypeLabels[i];
            break;
        }
    }

    QFont f = painter->font();
    f.setPointSize(8);
    painter->setFont(f);
    painter->setPen(nodeStyle.FontColorFaded);
    QRectF labelRect(0, size.height() - 18, size.width(), 16);
    painter->drawText(labelRect, Qt::AlignHCenter | Qt::AlignTop, typeStr);

    for (QtNodes::PortType portType : {QtNodes::PortType::Out, QtNodes::PortType::In}) {
        auto portCountRole = (portType == QtNodes::PortType::Out) ? QtNodes::NodeRole::OutPortCount
                                                                  : QtNodes::NodeRole::InPortCount;
        size_t const n = model.nodeData(nodeId, portCountRole).toUInt();

        for (QtNodes::PortIndex portIndex = 0; portIndex < n; ++portIndex) {
            QPointF p = geometry.portPosition(nodeId, portType, portIndex);

            auto const &connected = model.connections(nodeId, portType, portIndex);

            if (!connected.empty()) {
                painter->setPen(nodeStyle.FilledConnectionPointColor);
                painter->setBrush(nodeStyle.FilledConnectionPointColor);
            } else {
                painter->setPen(nodeStyle.ConnectionPointColor);
                painter->setBrush(nodeStyle.ConnectionPointColor);
            }

            double const diameter = nodeStyle.ConnectionPointDiameter;
            painter->drawEllipse(p, diameter * 0.4, diameter * 0.4);
        }
    }
}

MetroGraphModel::MetroGraphModel(MetroGraph &graph)
    : _graph(graph)
{
}

void MetroGraphModel::loadFromMetroGraph()
{
    _nodeMap.clear();
    _metroIdToNodeId.clear();
    _connections.clear();
    _edgeInfoMap.clear();
    _nextNodeId = 0;

    for (const auto &[metroId, node] : _graph.nodes()) {
        NodeId nid = _nextNodeId++;
        NodeInfo info;
        info.metroId = metroId;
        info.type = node.type.empty() ? "platform" : node.type;
        info.floor = node.floor;
        info.capacity = node.capacity;
        info.width = node.width;
        info.pos = QPointF(node.x * kCoordScale, node.y * kCoordScale);

        QString iconName = "node_" + QString::fromStdString(node.type) + ".svg";
        info.icon = iconName;

        _nodeMap[nid] = info;
        _metroIdToNodeId[metroId] = nid;
        Q_EMIT nodeCreated(nid);
    }

    for (std::size_t i = 0; i < _graph.edges().size(); ++i) {
        const auto &edge = _graph.edges()[i];
        auto fromIt = _metroIdToNodeId.find(edge.from);
        auto toIt = _metroIdToNodeId.find(edge.to);
        if (fromIt == _metroIdToNodeId.end() || toIt == _metroIdToNodeId.end())
            continue;

        ConnectionId cid{fromIt->second, 0, toIt->second, 0};
        _connections.insert(cid);

        EdgeInfo einfo;
        einfo.length = edge.length;
        einfo.width = edge.width;
        einfo.capacity = edge.capacity;
        einfo.transferTime = edge.transferTime;
        einfo.lineIndex = edge.lineIndex;
        einfo.bidirectional = edge.bidirectional;
        _edgeInfoMap[cid] = einfo;

        Q_EMIT connectionCreated(cid);
    }
}

void MetroGraphModel::saveToMetroGraph()
{
    _graph.clear();

    for (const auto &[nid, info] : _nodeMap) {
        StationNode node;
        node.id = info.metroId;
        node.name = info.metroId;
        node.type = info.type;
        node.x = info.pos.x() / kCoordScale;
        node.y = info.pos.y() / kCoordScale;
        node.capacity = info.capacity;
        node.width = info.width;
        node.floor = info.floor;

        QJsonObject internal = saveNode(nid);
        if (internal.contains("name"))
            node.name = internal["name"].toString().toStdString();

        _graph.addNode(node);
    }

    for (const auto &cid : _connections) {
        auto fromIt = _nodeMap.find(cid.outNodeId);
        auto toIt = _nodeMap.find(cid.inNodeId);
        if (fromIt == _nodeMap.end() || toIt == _nodeMap.end())
            continue;

        GraphEdge edge;
        edge.from = fromIt->second.metroId;
        edge.to = toIt->second.metroId;

        auto eit = _edgeInfoMap.find(cid);
        if (eit != _edgeInfoMap.end()) {
            edge.length = eit->second.length;
            edge.width = eit->second.width;
            edge.capacity = eit->second.capacity;
            edge.transferTime = eit->second.transferTime;
            edge.lineIndex = eit->second.lineIndex;
            edge.bidirectional = eit->second.bidirectional;
        } else {
            edge.length = 10.0;
            edge.width = 2.0;
            edge.capacity = 100.0;
            edge.transferTime = 1.0;
            edge.lineIndex = 1;
            edge.bidirectional = true;
        }

        _graph.addEdge(edge);
	}

	std::set<int> uniqueFloors;
	for (const auto &[nid, info] : _nodeMap) {
		uniqueFloors.insert(info.floor);
	}
	_graph.setFloors(std::vector<int>(uniqueFloors.begin(), uniqueFloors.end()));
}

const MetroGraphModel::NodeInfo *MetroGraphModel::nodeInfo(NodeId id) const
{
    auto it = _nodeMap.find(id);
    if (it != _nodeMap.end())
        return &it->second;
    return nullptr;
}

MetroGraphModel::EdgeInfo *MetroGraphModel::edgeInfo(ConnectionId cid)
{
    auto it = _edgeInfoMap.find(cid);
    if (it != _edgeInfoMap.end())
        return &it->second;
    return nullptr;
}

std::unordered_set<NodeId> MetroGraphModel::allNodeIds() const
{
    std::unordered_set<NodeId> result;
    for (const auto &[nid, info] : _nodeMap)
        result.insert(nid);
    return result;
}

std::unordered_set<ConnectionId> MetroGraphModel::allConnectionIds(NodeId nodeId) const
{
    std::unordered_set<ConnectionId> result;
    for (const auto &cid : _connections) {
        if (cid.inNodeId == nodeId || cid.outNodeId == nodeId)
            result.insert(cid);
    }
    return result;
}

std::unordered_set<ConnectionId> MetroGraphModel::connections(NodeId nodeId,
                                                               PortType portType,
                                                               PortIndex portIndex) const
{
    std::unordered_set<ConnectionId> result;
    for (const auto &cid : _connections) {
        if (portType == PortType::In && cid.inNodeId == nodeId && cid.inPortIndex == portIndex)
            result.insert(cid);
        else if (portType == PortType::Out && cid.outNodeId == nodeId && cid.outPortIndex == portIndex)
            result.insert(cid);
    }
    return result;
}

bool MetroGraphModel::connectionExists(ConnectionId const connectionId) const
{
    return _connections.find(connectionId) != _connections.end();
}

NodeId MetroGraphModel::addNode(QString const nodeType)
{
    NodeId newId = _nextNodeId++;

    std::string metroId = "node_" + std::to_string(newId);
    NodeInfo info;
    info.metroId = metroId;
    info.type = "platform";
    info.floor = 0;
    info.capacity = 200.0;
    info.width = 3.0;
    info.icon = "node_platform.svg";
    info.pos = QPointF(0, 0);

    _nodeMap[newId] = info;
    _metroIdToNodeId[metroId] = newId;

    Q_EMIT nodeCreated(newId);
    return newId;
}

bool MetroGraphModel::connectionPossible(ConnectionId const connectionId) const
{
    if (connectionId.outNodeId == connectionId.inNodeId)
        return false;
    if (_connections.find(connectionId) != _connections.end())
        return false;
    ConnectionId reversed{connectionId.inNodeId, 0, connectionId.outNodeId, 0};
    if (_connections.find(reversed) != _connections.end())
        return false;
    return true;
}

void MetroGraphModel::addConnection(ConnectionId const connectionId)
{
    _connections.insert(connectionId);
    _edgeInfoMap[connectionId] = EdgeInfo{};
    Q_EMIT connectionCreated(connectionId);
}

bool MetroGraphModel::nodeExists(NodeId const nodeId) const
{
    return _nodeMap.find(nodeId) != _nodeMap.end();
}

QVariant MetroGraphModel::nodeData(NodeId nodeId, NodeRole role) const
{
    auto it = _nodeMap.find(nodeId);
    if (it == _nodeMap.end())
        return {};

    switch (role) {
    case NodeRole::Type:
        return QString("StationNode");
    case NodeRole::Position:
        return it->second.pos;
    case NodeRole::Size:
        return QSize(160, 80);
    case NodeRole::CaptionVisible:
        return true;
    case NodeRole::Caption:
        return QString::fromStdString(it->second.metroId);
    case NodeRole::Style: {
        auto style = QtNodes::StyleCollection::nodeStyle();
        return style.toJson().toVariantMap();
    }
    case NodeRole::InternalData: {
        QJsonObject obj;
        obj["metroId"] = QString::fromStdString(it->second.metroId);
        obj["type"] = QString::fromStdString(it->second.type);
        obj["floor"] = it->second.floor;
        obj["capacity"] = it->second.capacity;
        obj["width"] = it->second.width;
        obj["icon"] = it->second.icon;
        return obj;
    }
    case NodeRole::InPortCount:
        return 1u;
    case NodeRole::OutPortCount:
        return 1u;
    default:
        return {};
    }
}

bool MetroGraphModel::setNodeData(NodeId nodeId, NodeRole role, QVariant value)
{
    auto it = _nodeMap.find(nodeId);
    if (it == _nodeMap.end())
        return false;

    switch (role) {
    case NodeRole::Position: {
        it->second.pos = value.value<QPointF>();
        Q_EMIT nodePositionUpdated(nodeId);
        return true;
    }
    case NodeRole::Size:
        return true;
    case NodeRole::Caption: {
        QString newName = value.toString();
        if (!newName.isEmpty()) {
            std::string oldId = it->second.metroId;
            _metroIdToNodeId.erase(oldId);
            it->second.metroId = newName.toStdString();
            _metroIdToNodeId[it->second.metroId] = nodeId;
            Q_EMIT nodeUpdated(nodeId);
            return true;
        }
        return false;
    }
    case NodeRole::InternalData: {
        QJsonObject obj = value.toJsonObject();
        if (obj.contains("metroId")) {
            std::string oldId = it->second.metroId;
            _metroIdToNodeId.erase(oldId);
            it->second.metroId = obj["metroId"].toString().toStdString();
            _metroIdToNodeId[it->second.metroId] = nodeId;
        }
        if (obj.contains("type"))
            it->second.type = obj["type"].toString().toStdString();
        if (obj.contains("floor"))
            it->second.floor = obj["floor"].toInt();
        if (obj.contains("capacity"))
            it->second.capacity = obj["capacity"].toDouble();
        if (obj.contains("width"))
            it->second.width = obj["width"].toDouble();
        if (obj.contains("icon"))
            it->second.icon = obj["icon"].toString();
        Q_EMIT nodeUpdated(nodeId);
        return true;
    }
    default:
        return false;
    }
}

QVariant MetroGraphModel::portData(NodeId nodeId,
                                    PortType portType,
                                    PortIndex portIndex,
                                    PortRole role) const
{
    Q_UNUSED(nodeId);
    Q_UNUSED(portIndex);

    switch (role) {
    case PortRole::Data:
        return QVariant();
    case PortRole::DataType:
        return QString("path");
    case PortRole::ConnectionPolicyRole:
        return QVariant::fromValue(QtNodes::ConnectionPolicy::Many);
    case PortRole::CaptionVisible:
        return true;
    case PortRole::Caption:
        if (portType == PortType::In)
            return QStringLiteral("入");
        else
            return QStringLiteral("出");
    }
    return {};
}

bool MetroGraphModel::setPortData(NodeId nodeId,
                                   PortType portType,
                                   PortIndex portIndex,
                                   QVariant const &value,
                                   PortRole role)
{
    Q_UNUSED(nodeId);
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    Q_UNUSED(value);
    Q_UNUSED(role);
    return false;
}

bool MetroGraphModel::deleteConnection(ConnectionId const connectionId)
{
    auto it = _connections.find(connectionId);
    if (it == _connections.end())
        return false;

    _connections.erase(it);
    _edgeInfoMap.erase(connectionId);
    Q_EMIT connectionDeleted(connectionId);
    return true;
}

bool MetroGraphModel::deleteNode(NodeId const nodeId)
{
    auto it = _nodeMap.find(nodeId);
    if (it == _nodeMap.end())
        return false;

    auto connIds = allConnectionIds(nodeId);
    for (auto &cid : connIds)
        deleteConnection(cid);

    _metroIdToNodeId.erase(it->second.metroId);
    _nodeMap.erase(it);
    Q_EMIT nodeDeleted(nodeId);
    return true;
}

QJsonObject MetroGraphModel::saveNode(NodeId nodeId) const
{
    auto it = _nodeMap.find(nodeId);
    QJsonObject nodeJson;
    nodeJson["id"] = static_cast<qint64>(nodeId);

    if (it != _nodeMap.end()) {
        nodeJson["metroId"] = QString::fromStdString(it->second.metroId);
        nodeJson["type"] = QString::fromStdString(it->second.type);
        nodeJson["floor"] = it->second.floor;
        nodeJson["capacity"] = it->second.capacity;
        nodeJson["width"] = it->second.width;
        nodeJson["icon"] = it->second.icon;

        QJsonObject posJson;
        posJson["x"] = it->second.pos.x();
        posJson["y"] = it->second.pos.y();
        nodeJson["position"] = posJson;
    }

    return nodeJson;
}

void MetroGraphModel::loadNode(QJsonObject const &nodeJson)
{
    NodeId restoredId = static_cast<NodeId>(nodeJson["id"].toInt());
    _nextNodeId = std::max(_nextNodeId, restoredId + 1);

    NodeInfo info;
    info.metroId = nodeJson["metroId"].toString().toStdString();
    if (info.metroId.empty())
        info.metroId = "node_" + std::to_string(restoredId);

    info.type = nodeJson["type"].toString("platform").toStdString();
    info.floor = nodeJson["floor"].toInt(0);
    info.capacity = nodeJson["capacity"].toDouble(200.0);
    info.width = nodeJson["width"].toDouble(3.0);
    info.icon = nodeJson["icon"].toString("node_platform.svg");

    if (nodeJson.contains("position")) {
        QJsonObject posJson = nodeJson["position"].toObject();
        info.pos = QPointF(posJson["x"].toDouble(), posJson["y"].toDouble());
    }

    _nodeMap[restoredId] = info;
    _metroIdToNodeId[info.metroId] = restoredId;
    Q_EMIT nodeCreated(restoredId);
}

NodeId MetroGraphModel::nodeIdForName(const std::string &name) const
{
    auto it = _metroIdToNodeId.find(name);
    if (it != _metroIdToNodeId.end())
        return it->second;
    return InvalidNodeId;
}

std::string MetroGraphModel::nodeNameForId(NodeId id) const
{
    auto it = _nodeMap.find(id);
    if (it != _nodeMap.end())
        return it->second.metroId;
    return {};
}

StationEditorWidget::StationEditorWidget(MetroGraph &graph, QWidget *parent)
    : QDialog(parent)
    , _graph(graph)
    , _model(nullptr)
    , _scene(nullptr)
    , _view(nullptr)
    , _toolbar(nullptr)
    , _rightPanel(nullptr)
    , _nodePropsGroup(nullptr)
    , _edgePropsGroup(nullptr)
    , _nodePropsPlaceholder(nullptr)
    , _edgePropsPlaceholder(nullptr)
    , _nodeNameEdit(nullptr)
    , _nodeTypeCombo(nullptr)
    , _nodeFloorSpin(nullptr)
    , _nodeCapacitySpin(nullptr)
    , _nodeWidthSpin(nullptr)
    , _nodeXSpin(nullptr)
    , _nodeYSpin(nullptr)
    , _nodeIconCombo(nullptr)
    , _edgeLengthSpin(nullptr)
    , _edgeWidthSpin(nullptr)
    , _edgeCapacitySpin(nullptr)
    , _edgeTransferSpin(nullptr)
    , _edgeLineSpin(nullptr)
    , _edgeBidirCheck(nullptr)
{
    setWindowTitle(QStringLiteral("拓扑编辑器"));
    resize(1300, 750);
    setMinimumSize(1000, 500);

    applyNodeStyle();

    _model = new MetroGraphModel(_graph);
    _model->setParent(this);
    _model->loadFromMetroGraph();

    _scene = new QtNodes::BasicGraphicsScene(*_model, this);
    _scene->setNodePainter(std::make_unique<StationNodePainter>());

    connect(_scene, &QtNodes::BasicGraphicsScene::nodeMoved, this,
            [this](NodeId nodeId, const QPointF &) {
                if (auto *ngo = _scene->nodeGraphicsObject(nodeId)) {
                    _model->setNodeData(nodeId, NodeRole::Position, ngo->pos());
                }
            });

    _view = new QtNodes::GraphicsView(_scene, this);
    _view->setRenderHint(QPainter::Antialiasing);
    _view->setDragMode(QGraphicsView::ScrollHandDrag);

    _view->setContextMenuPolicy(Qt::ActionsContextMenu);
    auto *addNodeAction = new QAction(QStringLiteral("添加节点"), _view);
    connect(addNodeAction, &QAction::triggered, this, [this]() {
        QPointF posView = _view->mapToScene(_view->mapFromGlobal(QCursor::pos()));
        NodeId newId = _model->addNode();
        _model->setNodeData(newId, NodeRole::Position, posView);
        _model->setNodeData(newId, NodeRole::Caption,
                            QStringLiteral("新节点%1").arg(newId));
    });
    _view->insertAction(_view->actions().isEmpty() ? nullptr : _view->actions().front(),
                        addNodeAction);

    auto *deleteAction = new QAction(QStringLiteral("删除选中"), _view);
    deleteAction->setShortcut(QKeySequence::Delete);
    connect(deleteAction, &QAction::triggered, this, &StationEditorWidget::onDeleteSelected);
    _view->insertAction(_view->actions().isEmpty() ? nullptr : _view->actions().front(),
                        deleteAction);
    _view->addAction(deleteAction);

    buildUi();

    connect(_scene, &QGraphicsScene::selectionChanged,
            this, &StationEditorWidget::onSelectionChanged);
}

StationEditorWidget::~StationEditorWidget()
{
    if (_scene) {
        disconnect(_scene, nullptr, this, nullptr);
    }
}

void StationEditorWidget::buildUi()
{
    _toolbar = new QToolBar(this);
    _toolbar->setMovable(false);
    _toolbar->setStyleSheet(
        QStringLiteral("QToolBar{background:#EDE4D8;border-bottom:1px solid #D4C5B2;padding:4px;}"
                       "QToolButton{background:#FEFAF3;color:#3D322C;border:1px solid #D4C5B2;"
                       "border-radius:6px;padding:5px 14px;font-weight:600;}"
                       "QToolButton:hover{background:#F5EDE0;border-color:#C43D3D;}"));

    auto *saveAction = _toolbar->addAction(QStringLiteral("保存并关闭"));
    connect(saveAction, &QAction::triggered, this, &StationEditorWidget::onSave);

    _toolbar->addSeparator();

    auto *addAction = _toolbar->addAction(QStringLiteral("添加节点"));
    connect(addAction, &QAction::triggered, this, &StationEditorWidget::onAddNode);

    auto *delAction = _toolbar->addAction(QStringLiteral("删除选中"));
    connect(delAction, &QAction::triggered, this, &StationEditorWidget::onDeleteSelected);

    _toolbar->addSeparator();

    auto *layoutAction = _toolbar->addAction(QStringLiteral("自动布局"));
    connect(layoutAction, &QAction::triggered, this, &StationEditorWidget::onAutoLayout);

    _toolbar->addSeparator();

    auto *helpLabel = new QLabel(QStringLiteral("拖拽节点移动 | 右键添加节点 | 从端口拖拽连线 | Delete删除"), this);
    helpLabel->setStyleSheet(QStringLiteral("color:#8B7D6B;font-size:12px;padding:0 8px;"));
    _toolbar->addWidget(helpLabel);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(_toolbar);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(_view);

    _rightPanel = new QWidget(this);
    _rightPanel->setFixedWidth(260);
    _rightPanel->setStyleSheet(
        QStringLiteral("QWidget#rightPanel{background:#F5F0E8;border-left:1px solid #D4C5B2;}"
                       "QGroupBox{background:#FEFAF3;border:1px solid #D4C5B2;border-radius:8px;"
                       "margin-top:12px;padding-top:16px;font-weight:bold;color:#3D322C;}"
                       "QGroupBox::title{subcontrol-origin:margin;left:12px;padding:0 6px;}"
                       "QLabel{color:#3D322C;font-size:12px;}"
                       "QLineEdit,QSpinBox,QDoubleSpinBox,QComboBox{background:#FFFFFF;"
                       "border:1px solid #D4C5B2;border-radius:4px;padding:3px 6px;"
                       "color:#3D322C;font-size:12px;}"
                       "QLineEdit:focus,QSpinBox:focus,QDoubleSpinBox:focus,QComboBox:focus{"
                       "border-color:#C43D3D;}"
                       "QPushButton{background:#FEFAF3;color:#3D322C;border:1px solid #D4C5B2;"
                       "border-radius:6px;padding:6px 12px;font-weight:600;font-size:12px;}"
                       "QPushButton:hover{background:#F5EDE0;border-color:#C43D3D;}"
                       "QPushButton#presetBtn{background:#C43D3D;color:#FFFFFF;border:none;"
                       "border-radius:6px;padding:8px 12px;font-weight:bold;font-size:13px;}"
                       "QPushButton#presetBtn:hover{background:#A83232;}"));
    _rightPanel->setObjectName("rightPanel");

    auto *scrollArea = new QScrollArea(_rightPanel);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("QScrollArea{background:transparent;}");

    auto *scrollContent = new QWidget(scrollArea);
    auto *panelLayout = new QVBoxLayout(scrollContent);
    panelLayout->setContentsMargins(10, 10, 10, 10);
    panelLayout->setSpacing(8);

    buildPresetSection(panelLayout);
    buildNodePropsSection(panelLayout);
    buildEdgePropsSection(panelLayout);

    panelLayout->addStretch();

    scrollArea->setWidget(scrollContent);

    auto *rightLayout = new QVBoxLayout(_rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(scrollArea);

    splitter->addWidget(_rightPanel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);

    mainLayout->addWidget(splitter, 1);

    QPointF center = _view->mapToScene(_view->viewport()->rect().center());
    _view->centerOn(center);
}

void StationEditorWidget::buildPresetSection(QVBoxLayout *layout)
{
    auto *presetGroup = new QGroupBox(QStringLiteral("预设方案"), _rightPanel);
    auto *presetLayout = new QVBoxLayout(presetGroup);
    presetLayout->setSpacing(6);

    auto *descLabel = new QLabel(QStringLiteral("一键生成标准车站拓扑："), presetGroup);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("color:#8B7D6B;font-size:11px;font-weight:normal;");
    presetLayout->addWidget(descLabel);

    auto *btnSmall = new QPushButton(QStringLiteral("20-30 节点（小型站）"), presetGroup);
    btnSmall->setObjectName("presetBtn");
    btnSmall->setToolTip(QStringLiteral("生成包含入口、安检、闸机、站台、出口等 20-30 个节点的标准小型车站拓扑"));
    connect(btnSmall, &QPushButton::clicked, this, &StationEditorWidget::onPresetSmall);
    presetLayout->addWidget(btnSmall);

    auto *btnLarge = new QPushButton(QStringLiteral("100+ 节点（大型站）"), presetGroup);
    btnLarge->setObjectName("presetBtn");
    btnLarge->setToolTip(QStringLiteral("生成包含多入口、多站台、多通道、换乘大厅等 100+ 个节点的复杂大型车站拓扑"));
    connect(btnLarge, &QPushButton::clicked, this, &StationEditorWidget::onPresetLarge);
    presetLayout->addWidget(btnLarge);

    auto *btnHuge = new QPushButton(QStringLiteral("1000+ 节点（超大型站）"), presetGroup);
    btnHuge->setObjectName("presetBtn");
    btnHuge->setToolTip(QStringLiteral("生成双翼三层超大型换乘站，1000+ 节点，用于压力测试"));
    connect(btnHuge, &QPushButton::clicked, this, &StationEditorWidget::onPresetHuge);
    presetLayout->addWidget(btnHuge);

    layout->addWidget(presetGroup);
}

void StationEditorWidget::buildNodePropsSection(QVBoxLayout *layout)
{
    _nodePropsGroup = new QGroupBox(QStringLiteral("节点属性"), _rightPanel);
    auto *formLayout = new QFormLayout(_nodePropsGroup);
    formLayout->setSpacing(6);
    formLayout->setContentsMargins(8, 8, 8, 8);

    _nodeNameEdit = new QLineEdit(_nodePropsGroup);
    _nodeNameEdit->setPlaceholderText(QStringLiteral("节点名称"));
    formLayout->addRow(QStringLiteral("名称:"), _nodeNameEdit);

    _nodeTypeCombo = new QComboBox(_nodePropsGroup);
    for (int i = 0; i < kNodeTypes.size(); ++i)
        _nodeTypeCombo->addItem(kNodeTypeLabels[i], kNodeTypes[i]);
    formLayout->addRow(QStringLiteral("类型:"), _nodeTypeCombo);

    _nodeFloorSpin = new QSpinBox(_nodePropsGroup);
    _nodeFloorSpin->setRange(-5, 10);
    _nodeFloorSpin->setSuffix(QStringLiteral(" 层"));
    formLayout->addRow(QStringLiteral("楼层:"), _nodeFloorSpin);

    _nodeCapacitySpin = new QDoubleSpinBox(_nodePropsGroup);
    _nodeCapacitySpin->setRange(0, 99999);
    _nodeCapacitySpin->setDecimals(1);
    _nodeCapacitySpin->setSuffix(QStringLiteral(" 人/min"));
    formLayout->addRow(QStringLiteral("容量:"), _nodeCapacitySpin);

    _nodeWidthSpin = new QDoubleSpinBox(_nodePropsGroup);
    _nodeWidthSpin->setRange(0.5, 100);
    _nodeWidthSpin->setDecimals(1);
    _nodeWidthSpin->setSuffix(QStringLiteral(" m"));
    formLayout->addRow(QStringLiteral("宽度:"), _nodeWidthSpin);

    _nodeXSpin = new QDoubleSpinBox(_nodePropsGroup);
    _nodeXSpin->setRange(-99999, 99999);
    _nodeXSpin->setDecimals(1);
    _nodeXSpin->setSuffix(QStringLiteral(" m"));
    formLayout->addRow(QStringLiteral("X 坐标:"), _nodeXSpin);

    _nodeYSpin = new QDoubleSpinBox(_nodePropsGroup);
    _nodeYSpin->setRange(-99999, 99999);
    _nodeYSpin->setDecimals(1);
    _nodeYSpin->setSuffix(QStringLiteral(" m"));
    formLayout->addRow(QStringLiteral("Y 坐标:"), _nodeYSpin);

    _nodeIconCombo = new QComboBox(_nodePropsGroup);
    _nodeIconCombo->setIconSize(QSize(20, 20));
    static const QHash<QString, QString> iconLabelMap = {
        {"node_entrance.svg",  QStringLiteral("入口")},
        {"node_exit.svg",      QStringLiteral("出口")},
        {"node_platform.svg",  QStringLiteral("站台")},
        {"node_corridor.svg",  QStringLiteral("通道")},
        {"node_stairs.svg",    QStringLiteral("楼梯")},
        {"node_escalator.svg", QStringLiteral("扶梯")},
        {"node_gate.svg",      QStringLiteral("闸机")},
        {"node_security.svg",  QStringLiteral("安检")},
        {"node_ticket.svg",    QStringLiteral("售票")},
        {"node_hall.svg",      QStringLiteral("大厅")},
        {"node_waiting.svg",   QStringLiteral("候车区")},
    };
    for (const auto &iconName : availableIcons()) {
        QString label = iconLabelMap.value(iconName, iconName);
        _nodeIconCombo->addItem(QIcon(resolveIconPath(iconName)), label, iconName);
    }
    formLayout->addRow(QStringLiteral("图标:"), _nodeIconCombo);

    _nodePropsPlaceholder = new QLabel(QStringLiteral("点击选中一个节点以编辑属性"), _nodePropsGroup);
    _nodePropsPlaceholder->setStyleSheet("color:#8B7D6B;font-size:11px;font-weight:normal;padding:4px;");
    _nodePropsPlaceholder->setWordWrap(true);
    formLayout->addRow(_nodePropsPlaceholder);

    connect(_nodeNameEdit, &QLineEdit::editingFinished, this, &StationEditorWidget::onNodePropertyChanged);
    connect(_nodeTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StationEditorWidget::onNodePropertyChanged);
    connect(_nodeFloorSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &StationEditorWidget::onNodePropertyChanged);
    connect(_nodeCapacitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &StationEditorWidget::onNodePropertyChanged);
    connect(_nodeWidthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &StationEditorWidget::onNodePropertyChanged);
    connect(_nodeXSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &StationEditorWidget::onNodePropertyChanged);
    connect(_nodeYSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &StationEditorWidget::onNodePropertyChanged);
    connect(_nodeIconCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StationEditorWidget::onNodePropertyChanged);

    layout->addWidget(_nodePropsGroup);
}

void StationEditorWidget::buildEdgePropsSection(QVBoxLayout *layout)
{
    _edgePropsGroup = new QGroupBox(QStringLiteral("连线属性"), _rightPanel);
    auto *formLayout = new QFormLayout(_edgePropsGroup);
    formLayout->setSpacing(6);
    formLayout->setContentsMargins(8, 8, 8, 8);

    _edgeLengthSpin = new QDoubleSpinBox(_edgePropsGroup);
    _edgeLengthSpin->setRange(0.1, 9999);
    _edgeLengthSpin->setDecimals(1);
    _edgeLengthSpin->setSuffix(QStringLiteral(" m"));
    formLayout->addRow(QStringLiteral("长度:"), _edgeLengthSpin);

    _edgeWidthSpin = new QDoubleSpinBox(_edgePropsGroup);
    _edgeWidthSpin->setRange(0.5, 100);
    _edgeWidthSpin->setDecimals(1);
    _edgeWidthSpin->setSuffix(QStringLiteral(" m"));
    formLayout->addRow(QStringLiteral("宽度:"), _edgeWidthSpin);

    _edgeCapacitySpin = new QDoubleSpinBox(_edgePropsGroup);
    _edgeCapacitySpin->setRange(0, 99999);
    _edgeCapacitySpin->setDecimals(1);
    _edgeCapacitySpin->setSuffix(QStringLiteral(" 人/min"));
    formLayout->addRow(QStringLiteral("容量:"), _edgeCapacitySpin);

    _edgeTransferSpin = new QDoubleSpinBox(_edgePropsGroup);
    _edgeTransferSpin->setRange(0, 999);
    _edgeTransferSpin->setDecimals(1);
    _edgeTransferSpin->setSuffix(QStringLiteral(" min"));
    formLayout->addRow(QStringLiteral("换乘时间:"), _edgeTransferSpin);

    _edgeLineSpin = new QSpinBox(_edgePropsGroup);
    _edgeLineSpin->setRange(1, 99);
    _edgeLineSpin->setPrefix(QStringLiteral("线路 "));
    formLayout->addRow(QStringLiteral("线路:"), _edgeLineSpin);

    _edgeBidirCheck = new QCheckBox(QStringLiteral("双向通行"), _edgePropsGroup);
    formLayout->addRow(QStringLiteral("方向:"), _edgeBidirCheck);

    _edgePropsPlaceholder = new QLabel(QStringLiteral("点击选中一条连线以编辑属性"), _edgePropsGroup);
    _edgePropsPlaceholder->setStyleSheet("color:#8B7D6B;font-size:11px;font-weight:normal;padding:4px;");
    _edgePropsPlaceholder->setWordWrap(true);
    formLayout->addRow(_edgePropsPlaceholder);

    connect(_edgeLengthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &StationEditorWidget::onEdgePropertyChanged);
    connect(_edgeWidthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &StationEditorWidget::onEdgePropertyChanged);
    connect(_edgeCapacitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &StationEditorWidget::onEdgePropertyChanged);
    connect(_edgeTransferSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &StationEditorWidget::onEdgePropertyChanged);
    connect(_edgeLineSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &StationEditorWidget::onEdgePropertyChanged);
    connect(_edgeBidirCheck, &QCheckBox::toggled,
            this, &StationEditorWidget::onEdgePropertyChanged);

    layout->addWidget(_edgePropsGroup);
}

QStringList StationEditorWidget::availableIcons() const
{
    return {
        "node_entrance.svg",
        "node_exit.svg",
        "node_platform.svg",
        "node_corridor.svg",
        "node_stairs.svg",
        "node_escalator.svg",
        "node_gate.svg",
        "node_security.svg",
        "node_ticket.svg",
        "node_hall.svg",
        "node_waiting.svg",
    };
}

void StationEditorWidget::applyNodeStyle()
{
    QtNodes::NodeStyle nodeStyle;
    nodeStyle.NormalBoundaryColor = QColor("#D4C5B2");
    nodeStyle.SelectedBoundaryColor = QColor("#C43D3D");
    nodeStyle.GradientColor0 = QColor("#FEFAF3");
    nodeStyle.GradientColor1 = QColor("#FDF8F0");
    nodeStyle.GradientColor2 = QColor("#F5EDE0");
    nodeStyle.GradientColor3 = QColor("#EDE4D8");
    nodeStyle.ShadowColor = QColor(139, 94, 122, 45);
    nodeStyle.ShadowEnabled = false;
    nodeStyle.FontColor = QColor("#3D322C");
    nodeStyle.FontColorFaded = QColor("#8B7D6B");
    nodeStyle.ConnectionPointColor = QColor("#D4C5B2");
    nodeStyle.FilledConnectionPointColor = QColor("#C43D3D");
    nodeStyle.PenWidth = 1.5f;
    nodeStyle.HoveredPenWidth = 2.5f;
    nodeStyle.ConnectionPointDiameter = 10.0;
    nodeStyle.Opacity = 0.98f;
    QtNodes::StyleCollection::setNodeStyle(nodeStyle);

    QtNodes::ConnectionStyle connStyle;
    connStyle.loadJson(QJsonObject{
        {"ConstructionColor", QJsonValue::fromVariant(QColor("#4A9C8C"))},
        {"NormalColor", QJsonValue::fromVariant(QColor("#4A9C8C"))},
        {"SelectedColor", QJsonValue::fromVariant(QColor("#C43D3D"))},
        {"SelectedHaloColor", QJsonValue::fromVariant(QColor(196, 61, 61, 80))},
        {"HoveredColor", QJsonValue::fromVariant(QColor("#D4953A"))},
        {"LineWidth", 2.5},
        {"ConstructionLineWidth", 2.0},
        {"PointDiameter", 8.0},
        {"UseDataDefinedColors", false},
    });
    QtNodes::StyleCollection::setConnectionStyle(connStyle);

    QtNodes::GraphicsViewStyle viewStyle;
    viewStyle.BackgroundColor = QColor("#F5F0E8");
    viewStyle.FineGridColor = QColor(220, 210, 195, 60);
    viewStyle.CoarseGridColor = QColor(200, 185, 165, 100);
    QtNodes::StyleCollection::setGraphicsViewStyle(viewStyle);
}

void StationEditorWidget::onSave()
{
    _model->saveToMetroGraph();
    accept();
}

void StationEditorWidget::onAddNode()
{
    QPointF center = _view->mapToScene(_view->viewport()->rect().center());
    NodeId newId = _model->addNode();
    _model->setNodeData(newId, NodeRole::Position, center);
    _model->setNodeData(newId, NodeRole::Caption,
                        QStringLiteral("新节点%1").arg(newId));
}

void StationEditorWidget::onDeleteSelected()
{
    auto selectedNodes = _scene->selectedNodes();
    for (auto *nodeObj : selectedNodes) {
        _model->deleteNode(nodeObj->nodeId());
    }
    clearNodeProps();
    clearEdgeProps();
}

void StationEditorWidget::onSelectionChanged()
{
    if (!_scene)
        return;

    auto selectedNodes = _scene->selectedNodes();

    ConnectionId selectedCid{InvalidNodeId, 0, InvalidNodeId, 0};
    bool hasConnection = false;
    for (auto *item : _scene->selectedItems()) {
        auto *connObj = dynamic_cast<QtNodes::ConnectionGraphicsObject *>(item);
        if (connObj) {
            selectedCid = connObj->connectionId();
            hasConnection = true;
            break;
        }
    }

    if (selectedNodes.size() == 1 && !hasConnection) {
        NodeId nid = selectedNodes.front()->nodeId();
        refreshNodeProps(nid);
        clearEdgeProps();
    } else if (hasConnection && selectedNodes.empty()) {
        refreshEdgeProps(selectedCid);
        clearNodeProps();
    } else {
        clearNodeProps();
        clearEdgeProps();
    }
}

void StationEditorWidget::refreshNodeProps(NodeId nodeId)
{
    _updatingProps = true;
    _currentNodeId = nodeId;

    const auto *info = _model->nodeInfo(nodeId);
    if (!info) {
        _updatingProps = false;
        return;
    }

    _nodeNameEdit->setText(QString::fromStdString(info->metroId));
    _nodeNameEdit->setVisible(true);

    int typeIdx = _nodeTypeCombo->findData(QString::fromStdString(info->type));
    if (typeIdx >= 0)
        _nodeTypeCombo->setCurrentIndex(typeIdx);

    _nodeFloorSpin->setValue(info->floor);
    _nodeCapacitySpin->setValue(info->capacity);
    _nodeWidthSpin->setValue(info->width);
    _nodeXSpin->setValue(info->pos.x() / kCoordScale);
    _nodeYSpin->setValue(info->pos.y() / kCoordScale);

    int iconIdx = _nodeIconCombo->findData(info->icon);
    if (iconIdx >= 0)
        _nodeIconCombo->setCurrentIndex(iconIdx);

    _nodePropsPlaceholder->setVisible(false);

    _nodeNameEdit->setEnabled(true);
    _nodeTypeCombo->setEnabled(true);
    _nodeFloorSpin->setEnabled(true);
    _nodeCapacitySpin->setEnabled(true);
    _nodeWidthSpin->setEnabled(true);
    _nodeXSpin->setEnabled(true);
    _nodeYSpin->setEnabled(true);
    _nodeIconCombo->setEnabled(true);

    _updatingProps = false;
}

void StationEditorWidget::refreshEdgeProps(ConnectionId cid)
{
    _updatingProps = true;
    _currentEdgeId = cid;

    auto *einfo = _model->edgeInfo(cid);
    if (!einfo) {
        _updatingProps = false;
        return;
    }

    _edgeLengthSpin->setValue(einfo->length);
    _edgeWidthSpin->setValue(einfo->width);
    _edgeCapacitySpin->setValue(einfo->capacity);
    _edgeTransferSpin->setValue(einfo->transferTime);
    _edgeLineSpin->setValue(einfo->lineIndex);
    _edgeBidirCheck->setChecked(einfo->bidirectional);

    _edgePropsPlaceholder->setVisible(false);

    _edgeLengthSpin->setEnabled(true);
    _edgeWidthSpin->setEnabled(true);
    _edgeCapacitySpin->setEnabled(true);
    _edgeTransferSpin->setEnabled(true);
    _edgeLineSpin->setEnabled(true);
    _edgeBidirCheck->setEnabled(true);

    _updatingProps = false;
}

void StationEditorWidget::clearNodeProps()
{
    _updatingProps = true;
    _currentNodeId = InvalidNodeId;

    _nodeNameEdit->clear();
    _nodeNameEdit->setEnabled(false);
    _nodeTypeCombo->setEnabled(false);
    _nodeFloorSpin->setEnabled(false);
    _nodeCapacitySpin->setEnabled(false);
    _nodeWidthSpin->setEnabled(false);
    _nodeXSpin->setEnabled(false);
    _nodeYSpin->setEnabled(false);
    _nodeIconCombo->setEnabled(false);
    _nodePropsPlaceholder->setVisible(true);

    _updatingProps = false;
}

void StationEditorWidget::clearEdgeProps()
{
    _updatingProps = true;
    _currentEdgeId = {InvalidNodeId, 0, InvalidNodeId, 0};

    _edgeLengthSpin->setEnabled(false);
    _edgeWidthSpin->setEnabled(false);
    _edgeCapacitySpin->setEnabled(false);
    _edgeTransferSpin->setEnabled(false);
    _edgeLineSpin->setEnabled(false);
    _edgeBidirCheck->setEnabled(false);
    _edgePropsPlaceholder->setVisible(true);

    _updatingProps = false;
}

void StationEditorWidget::onNodePropertyChanged()
{
    if (_updatingProps || _currentNodeId == InvalidNodeId)
        return;

    auto *info = const_cast<MetroGraphModel::NodeInfo *>(_model->nodeInfo(_currentNodeId));
    if (!info)
        return;

    QString newName = _nodeNameEdit->text().trimmed();
    if (!newName.isEmpty() && newName.toStdString() != info->metroId) {
        _model->setNodeData(_currentNodeId, NodeRole::Caption, newName);
    }

    QJsonObject obj;
    obj["metroId"] = QString::fromStdString(info->metroId);
    obj["name"] = newName.isEmpty() ? QString::fromStdString(info->metroId) : newName;
    obj["type"] = _nodeTypeCombo->currentData().toString();
    obj["floor"] = _nodeFloorSpin->value();
    obj["capacity"] = _nodeCapacitySpin->value();
    obj["width"] = _nodeWidthSpin->value();
    obj["icon"] = _nodeIconCombo->currentData().toString();
    _model->setNodeData(_currentNodeId, NodeRole::InternalData, obj);

    if (auto *ngo = _scene->nodeGraphicsObject(_currentNodeId)) {
        ngo->update();
    }

    QPointF newPos(_nodeXSpin->value() * kCoordScale,
                   _nodeYSpin->value() * kCoordScale);
    if (newPos != info->pos) {
        _model->setNodeData(_currentNodeId, NodeRole::Position, newPos);
    }
}

void StationEditorWidget::onEdgePropertyChanged()
{
    if (_updatingProps)
        return;
    if (_currentEdgeId.outNodeId == InvalidNodeId || _currentEdgeId.inNodeId == InvalidNodeId)
        return;

    auto *einfo = _model->edgeInfo(_currentEdgeId);
    if (!einfo)
        return;

    einfo->length = _edgeLengthSpin->value();
    einfo->width = _edgeWidthSpin->value();
    einfo->capacity = _edgeCapacitySpin->value();
    einfo->transferTime = _edgeTransferSpin->value();
    einfo->lineIndex = _edgeLineSpin->value();
    einfo->bidirectional = _edgeBidirCheck->isChecked();
}

void StationEditorWidget::onPresetSmall()
{
	auto result = QMessageBox::question(
		this,
		QStringLiteral("加载预设方案"),
		QStringLiteral("将清空当前拓扑并加载「20-30 节点小型站」预设方案，是否继续？"),
		QMessageBox::Yes | QMessageBox::No,
		QMessageBox::No);

	if (result != QMessageBox::Yes)
		return;

	std::vector<std::tuple<double, double, QString, QString>> nodes;
	std::vector<std::tuple<int, int>> edges;

	nodes.push_back({0, 0, "entrance", "node_entrance.svg"});
	nodes.push_back({3, 0, "entrance", "node_entrance.svg"});
	nodes.push_back({6, 0, "entrance", "node_entrance.svg"});
	nodes.push_back({9, 0, "entrance", "node_entrance.svg"});

	nodes.push_back({1, 5, "security", "node_security.svg"});
	nodes.push_back({7, 5, "security", "node_security.svg"});

	nodes.push_back({1, 10, "ticket", "node_ticket.svg"});
	nodes.push_back({7, 10, "ticket", "node_ticket.svg"});

	nodes.push_back({0, 15, "gate", "node_gate.svg"});
	nodes.push_back({4, 15, "gate", "node_gate.svg"});
	nodes.push_back({8, 15, "gate", "node_gate.svg"});

	nodes.push_back({4, 20, "hall", "node_hall.svg"});

	nodes.push_back({1, 25, "corridor", "node_corridor.svg"});
	nodes.push_back({7, 25, "corridor", "node_corridor.svg"});

	nodes.push_back({1, 30, "stairs", "node_stairs.svg"});
	nodes.push_back({7, 30, "escalator", "node_escalator.svg"});

	nodes.push_back({1, 35, "waiting", "node_waiting.svg"});
	nodes.push_back({7, 35, "waiting", "node_waiting.svg"});

	nodes.push_back({1, 40, "platform", "node_platform.svg"});
	nodes.push_back({7, 40, "platform", "node_platform.svg"});

	nodes.push_back({1, 45, "stairs", "node_stairs.svg"});
	nodes.push_back({7, 45, "escalator", "node_escalator.svg"});

	nodes.push_back({4, 50, "hall", "node_hall.svg"});

	nodes.push_back({1, 55, "gate", "node_gate.svg"});
	nodes.push_back({7, 55, "gate", "node_gate.svg"});

	nodes.push_back({1, 60, "exit", "node_exit.svg"});
	nodes.push_back({7, 60, "exit", "node_exit.svg"});

	edges.push_back({0, 4}); edges.push_back({1, 4});
	edges.push_back({2, 5}); edges.push_back({3, 5});
	edges.push_back({4, 6});
	edges.push_back({5, 7});
	edges.push_back({6, 8}); edges.push_back({6, 9});
	edges.push_back({7, 9}); edges.push_back({7, 10});
	edges.push_back({8, 11}); edges.push_back({9, 11}); edges.push_back({10, 11});
	edges.push_back({11, 12}); edges.push_back({11, 13});
	edges.push_back({12, 14});
	edges.push_back({13, 15});
	edges.push_back({14, 16});
	edges.push_back({15, 17});
	edges.push_back({16, 18});
	edges.push_back({17, 19});
	edges.push_back({18, 20}); edges.push_back({19, 20});
	edges.push_back({20, 21});
	edges.push_back({20, 22}); edges.push_back({21, 22});
	edges.push_back({21, 23});
	edges.push_back({22, 24});
	edges.push_back({23, 25});
	edges.push_back({24, 25});

	loadPreset(nodes, edges);
}

void StationEditorWidget::onPresetLarge()
{
	auto result = QMessageBox::question(
		this,
		QStringLiteral("加载预设方案"),
		QStringLiteral("将清空当前拓扑并加载「100+ 节点大型站」预设方案，是否继续？"),
		QMessageBox::Yes | QMessageBox::No,
		QMessageBox::No);

	if (result != QMessageBox::Yes)
		return;

	std::vector<std::tuple<double, double, QString, QString>> nodes;
	std::vector<std::tuple<int, int>> edges;

	struct RowSpec {
		double y;
		int count;
		double spacing;
		QString type;
		QString icon;
	};

	const std::vector<RowSpec> rows = {
		{0,  12, 4.0, "entrance",  "node_entrance.svg"},
		{5,  6,  8.0, "security",  "node_security.svg"},
		{10, 6,  8.0, "ticket",    "node_ticket.svg"},
		{15, 10, 4.5, "gate",      "node_gate.svg"},
		{20, 3,  16.0,"hall",      "node_hall.svg"},
		{25, 6,  8.0, "corridor",  "node_corridor.svg"},
		{30, 6,  8.0, "stairs",    "node_stairs.svg"},
		{35, 6,  8.0, "escalator", "node_escalator.svg"},
		{40, 6,  8.0, "waiting",   "node_waiting.svg"},
		{45, 6,  8.0, "platform",  "node_platform.svg"},
		{50, 6,  8.0, "stairs",    "node_stairs.svg"},
		{55, 6,  8.0, "escalator", "node_escalator.svg"},
		{60, 3,  16.0,"hall",      "node_hall.svg"},
		{65, 8,  5.5, "gate",      "node_gate.svg"},
		{70, 8,  5.5, "exit",      "node_exit.svg"},
	};

	std::vector<int> rowStart(rows.size(), 0);
	int total = 0;
	for (std::size_t r = 0; r < rows.size(); ++r) {
		rowStart[r] = total;
		double startX = -(rows[r].count - 1) * rows[r].spacing / 2.0;
		for (int i = 0; i < rows[r].count; ++i) {
			double x = startX + i * rows[r].spacing;
			nodes.push_back({x, rows[r].y, rows[r].type, rows[r].icon});
		}
		total += rows[r].count;
	}

	auto connectManyToOne = [&](int fromRow, int toRow) {
		int fromCount = rows[fromRow].count;
		int toCount = rows[toRow].count;
		int groupSize = fromCount / toCount;
		for (int t = 0; t < toCount; ++t) {
			int start = t * groupSize;
			int end = (t == toCount - 1) ? fromCount : start + groupSize;
			for (int f = start; f < end; ++f)
				edges.push_back({rowStart[fromRow] + f, rowStart[toRow] + t});
		}
	};

	auto connectOneToMany = [&](int fromRow, int toRow) {
		int fromCount = rows[fromRow].count;
		int toCount = rows[toRow].count;
		int groupSize = toCount / fromCount;
		for (int f = 0; f < fromCount; ++f) {
			int start = f * groupSize;
			int end = (f == fromCount - 1) ? toCount : start + groupSize;
			for (int t = start; t < end; ++t)
				edges.push_back({rowStart[fromRow] + f, rowStart[toRow] + t});
		}
	};

	auto connectOneToOne = [&](int fromRow, int toRow) {
		int count = std::min(rows[fromRow].count, rows[toRow].count);
		for (int i = 0; i < count; ++i)
			edges.push_back({rowStart[fromRow] + i, rowStart[toRow] + i});
	};

	connectManyToOne(0, 1);
	connectOneToOne(1, 2);
	connectOneToMany(2, 3);
	connectManyToOne(3, 4);
	connectOneToMany(4, 5);
	connectOneToOne(5, 6);
	connectOneToOne(5, 7);
	connectOneToOne(6, 8);
	connectOneToOne(7, 8);
	connectOneToOne(8, 9);

	for (int i = 0; i < rows[9].count; ++i) {
		for (int j = i + 1; j < rows[9].count; ++j) {
			edges.push_back({rowStart[9] + i, rowStart[9] + j});
		}
	}

	connectOneToOne(9, 10);
	connectOneToOne(9, 11);
	connectManyToOne(10, 12);
	connectManyToOne(11, 12);
	connectOneToMany(12, 13);
	connectOneToOne(13, 14);

	loadPreset(nodes, edges);
}

void StationEditorWidget::loadPreset(
    const std::vector<std::tuple<double, double, QString, QString>> &nodes,
    const std::vector<std::tuple<int, int>> &edges)
{
    _model->blockSignals(true);

    for (auto &nid : _model->allNodeIds())
        _model->deleteNode(nid);

    std::vector<NodeId> nodeIds;
    QMap<QString, int> typeCounters;
    for (const auto &[x, y, type, icon] : nodes) {
        NodeId nid = _model->addNode();
        double px = x * kCoordScale;
        double py = y * kCoordScale;
        _model->setNodeData(nid, NodeRole::Position, QPointF(px, py));

        int &counter = typeCounters[type];
        ++counter;
        QString chName = QStringLiteral("%1%2").arg(chineseLabelForType(type)).arg(counter);

        QJsonObject obj;
        obj["metroId"] = chName;
        obj["type"] = type;
        obj["name"] = chName;
        obj["floor"] = 0;
        obj["capacity"] = 200.0;
        obj["width"] = 3.0;
        obj["icon"] = icon;
        _model->setNodeData(nid, NodeRole::InternalData, obj);
        _model->setNodeData(nid, NodeRole::Caption, chName);

        nodeIds.push_back(nid);
    }

    for (const auto &[fromIdx, toIdx] : edges) {
        if (fromIdx < static_cast<int>(nodeIds.size())
            && toIdx < static_cast<int>(nodeIds.size())) {
            ConnectionId cid{nodeIds[fromIdx], 0, nodeIds[toIdx], 0};
            if (_model->connectionPossible(cid))
                _model->addConnection(cid);
        }
    }

    _model->blockSignals(false);

    _scene->onModelReset();

    _view->fitInView(_scene->sceneRect(), Qt::KeepAspectRatio);
    clearNodeProps();
    clearEdgeProps();
}

void StationEditorWidget::loadPreset(
    const std::vector<PresetNodeInfo> &nodes,
    const std::vector<std::tuple<int, int>> &edges)
{
    _model->blockSignals(true);

    auto oldIds = _model->allNodeIds();
    for (auto &nid : oldIds)
        _model->deleteNode(nid);

    std::vector<NodeId> nodeIds;
    QMap<QString, int> typeCounters;
    std::set<int> allFloors;

    for (const auto &n : nodes) {
        NodeId nid = _model->addNode();
        double px = n.x * kCoordScale;
        double py = n.y * kCoordScale;
        _model->setNodeData(nid, NodeRole::Position, QPointF(px, py));

        int &counter = typeCounters[n.type];
        ++counter;
        QString chName = QStringLiteral("%1%2").arg(chineseLabelForType(n.type)).arg(counter);

        QJsonObject obj;
        obj["metroId"] = chName;
        obj["type"] = n.type;
        obj["name"] = chName;
        obj["floor"] = n.floor;
        obj["capacity"] = n.capacity;
        obj["width"] = n.width;
        obj["icon"] = n.icon;
        _model->setNodeData(nid, NodeRole::InternalData, obj);
        _model->setNodeData(nid, NodeRole::Caption, chName);

        nodeIds.push_back(nid);
        allFloors.insert(n.floor);
    }

    for (const auto &[fromIdx, toIdx] : edges) {
        if (fromIdx < static_cast<int>(nodeIds.size())
            && toIdx < static_cast<int>(nodeIds.size())) {
            ConnectionId cid{nodeIds[fromIdx], 0, nodeIds[toIdx], 0};
            if (_model->connectionPossible(cid))
                _model->addConnection(cid);
        }
    }

    std::vector<int> floors(allFloors.begin(), allFloors.end());
    _graph.setFloors(floors);

    _model->blockSignals(false);

    disconnect(_scene, nullptr, this, nullptr);

    delete _scene;
    _scene = new QtNodes::BasicGraphicsScene(*_model, this);
    _scene->setNodePainter(std::make_unique<StationNodePainter>());
    _view->setScene(_scene);

    connect(_scene, &QtNodes::BasicGraphicsScene::nodeMoved, this,
            [this](NodeId nodeId, const QPointF &) {
                if (auto *ngo = _scene->nodeGraphicsObject(nodeId)) {
                    _model->setNodeData(nodeId, NodeRole::Position, ngo->pos());
                }
            });

    connect(_scene, &QGraphicsScene::selectionChanged,
            this, &StationEditorWidget::onSelectionChanged);

    _view->fitInView(_scene->sceneRect(), Qt::KeepAspectRatio);
    clearNodeProps();
    clearEdgeProps();
}

void StationEditorWidget::onPresetHuge()
{
    auto result = QMessageBox::question(
        this,
        QStringLiteral("加载预设方案"),
        QStringLiteral("将清空当前拓扑并加载「1000+ 节点超大型站」预设方案，生成可能需要几秒，是否继续？"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (result != QMessageBox::Yes)
        return;

    std::vector<PresetNodeInfo> nodes;
    std::vector<std::tuple<int, int>> edges;

    struct RowSpec {
        double yBase;
        int count;
        double spacing;
        QString type;
        QString icon;
        int floor;
        double capacity;
        double width;
    };

    const std::vector<RowSpec> wingRows = {
        {0,   40, 1.8, "entrance",  "node_entrance.svg",  0,  50.0, 4.0},
        {5,   20, 3.6, "security",  "node_security.svg",  0,  40.0, 3.0},
        {10,  20, 3.6, "ticket",    "node_ticket.svg",    0,  60.0, 3.0},
        {15,  30, 2.4, "gate",      "node_gate.svg",      0,  45.0, 2.5},
        {20,  10, 7.2, "hall",      "node_hall.svg",      0, 150.0, 5.0},
        {25,  20, 3.6, "corridor",  "node_corridor.svg",  0,  80.0, 4.0},
        {30,  10, 7.2, "exit",      "node_exit.svg",      0,  50.0, 4.0},
        {35,  10, 7.2, "stairs",    "node_stairs.svg",    0,  30.0, 2.5},
        {35,  10, 7.2, "escalator", "node_escalator.svg", 0,  35.0, 3.0},
        // B1
        {45,  15, 4.8, "corridor",  "node_corridor.svg", -1,  80.0, 4.0},
        {50,  10, 7.2, "waiting",   "node_waiting.svg",  -1, 120.0, 5.0},
        {55,  10, 7.2, "platform",  "node_platform.svg", -1, 250.0, 6.0},
        {60,   8, 9.0, "stairs",    "node_stairs.svg",   -1,  30.0, 2.5},
        {60,   8, 9.0, "escalator", "node_escalator.svg",-1,  35.0, 3.0},
        // B2
        {70,  15, 4.8, "corridor",  "node_corridor.svg", -2,  80.0, 4.0},
        {75,  10, 7.2, "waiting",   "node_waiting.svg",  -2, 120.0, 5.0},
        {80,  10, 7.2, "platform",  "node_platform.svg", -2, 250.0, 6.0},
    };

    auto addWing = [&](double xOffset) -> std::vector<std::vector<int>> {
        std::vector<std::vector<int>> rowIndices;
        for (const auto &row : wingRows) {
            std::vector<int> indices;
            double startX = xOffset - (row.count - 1) * row.spacing / 2.0;
            for (int i = 0; i < row.count; ++i) {
                int idx = static_cast<int>(nodes.size());
                double x = startX + i * row.spacing;
                nodes.push_back({x, row.yBase, row.type, row.icon, row.floor, row.capacity, row.width});
                indices.push_back(idx);
            }
            rowIndices.push_back(std::move(indices));
        }
        return rowIndices;
    };

    auto connectManyToFew = [&](const std::vector<int> &from, const std::vector<int> &to) {
        int fCount = static_cast<int>(from.size());
        int tCount = static_cast<int>(to.size());
        if (fCount == 0 || tCount == 0) return;
        for (int f = 0; f < fCount; ++f) {
            int t = f * tCount / fCount;
            edges.push_back({from[f], to[t]});
        }
    };

    auto connectFewToMany = [&](const std::vector<int> &from, const std::vector<int> &to) {
        int fCount = static_cast<int>(from.size());
        int tCount = static_cast<int>(to.size());
        if (fCount == 0 || tCount == 0) return;
        for (int t = 0; t < tCount; ++t) {
            int f = t * fCount / tCount;
            edges.push_back({from[f], to[t]});
        }
    };

    auto connectPairs = [&](const std::vector<int> &from, const std::vector<int> &to) {
        int count = static_cast<int>(std::min(from.size(), to.size()));
        for (int i = 0; i < count; ++i)
            edges.push_back({from[i], to[i]});
    };

    auto connectWing = [&](const std::vector<std::vector<int>> &r) {
        // F0: entrance(0)->security(1)->ticket(2)->gate(3)->hall(4)->corridor(5)->exit(6)
        //     hall(4)->stairs(7), escalator(8)
        connectManyToFew(r[0], r[1]);
        connectPairs(r[1], r[2]);
        connectFewToMany(r[2], r[3]);
        connectManyToFew(r[3], r[4]);
        connectFewToMany(r[4], r[5]);
        connectManyToFew(r[5], r[6]);
        connectFewToMany(r[4], r[7]);
        connectFewToMany(r[4], r[8]);
        // F0 stairs/escalator -> B1 corridor(9)
        connectManyToFew(r[7], r[9]);
        connectManyToFew(r[8], r[9]);
        // B1: corridor(9)->waiting(10)->platform(11)
        connectFewToMany(r[9], r[10]);
        connectPairs(r[10], r[11]);
        // B1 stairs/escalator(12,13) -> B2 corridor(14)
        connectFewToMany(r[9], r[12]);
        connectFewToMany(r[9], r[13]);
        connectManyToFew(r[12], r[14]);
        connectManyToFew(r[13], r[14]);
        // B2: corridor(14)->waiting(15)->platform(16)
        connectFewToMany(r[14], r[15]);
        connectPairs(r[15], r[16]);
    };

    auto leftWing = addWing(-45.0);
    auto rightWing = addWing(45.0);

    connectWing(leftWing);
    connectWing(rightWing);

    // Cross-connections between wings at each level
    auto addCrossLinks = [&](const std::vector<int> &leftRow, const std::vector<int> &rightRow, int count) {
        int lSize = static_cast<int>(leftRow.size());
        int rSize = static_cast<int>(rightRow.size());
        int step = std::max(1, std::min(lSize, rSize) / (count + 1));
        for (int i = 0; i < count; ++i) {
            int lIdx = std::min((i + 1) * step, lSize - 1);
            int rIdx = std::min((i + 1) * step, rSize - 1);
            int bridgeIdx = static_cast<int>(nodes.size());
            double mx = 0.0;
            double my = (nodes[leftRow[lIdx]].y + nodes[rightRow[rIdx]].y) / 2.0;
            int floor = nodes[leftRow[lIdx]].floor;
            nodes.push_back({mx, my, "corridor", "node_corridor.svg", floor, 100.0, 5.0});
            edges.push_back({leftRow[lIdx], bridgeIdx});
            edges.push_back({bridgeIdx, rightRow[rIdx]});
        }
    };

    addCrossLinks(leftWing[4], rightWing[4], 5);    // F0 hall
    addCrossLinks(leftWing[9], rightWing[9], 5);     // B1 corridor
    addCrossLinks(leftWing[14], rightWing[14], 5);   // B2 corridor

    loadPreset(nodes, edges);
}

void StationEditorWidget::onAutoLayout()
{
    const auto &nodeMap = _model->nodeMap();
    const auto &connections = _model->allConnections();
    if (nodeMap.empty())
        return;

    QMap<NodeId, QVector<NodeId>> adjOut;
    QMap<NodeId, QVector<NodeId>> adjIn;
    QMap<NodeId, int> inDegree;
    for (const auto &cid : connections) {
        adjOut[cid.outNodeId].append(cid.inNodeId);
        adjIn[cid.inNodeId].append(cid.outNodeId);
        inDegree[cid.inNodeId]++;
    }

    QMap<NodeId, int> layer;
    QQueue<NodeId> queue;
    QSet<NodeId> visited;

    for (const auto &[nid, info] : nodeMap) {
        if (inDegree.value(nid, 0) == 0) {
            layer[nid] = 0;
            queue.enqueue(nid);
            visited.insert(nid);
        }
    }

    if (queue.isEmpty() && !nodeMap.empty()) {
        NodeId first = nodeMap.begin()->first;
        layer[first] = 0;
        queue.enqueue(first);
        visited.insert(first);
    }

    while (!queue.isEmpty()) {
        NodeId cur = queue.dequeue();
        int curLayer = layer[cur];
        QString curType = QString::fromStdString(nodeMap.at(cur).type);
        for (NodeId next : adjOut.value(cur)) {
            if (visited.contains(next))
                continue;
            QString nextType = QString::fromStdString(nodeMap.at(next).type);
            int nextLayer = (curType == nextType) ? curLayer : curLayer + 1;
            layer[next] = nextLayer;
            visited.insert(next);
            queue.enqueue(next);
        }
    }

    for (const auto &[nid, info] : nodeMap) {
        if (!visited.contains(nid)) {
            int bestLayer = 0;
            for (NodeId pred : adjIn.value(nid)) {
                if (layer.contains(pred)) {
                    QString predType = QString::fromStdString(nodeMap.at(pred).type);
                    QString myType = QString::fromStdString(info.type);
                    int candidate = (predType == myType) ? layer[pred] : layer[pred] + 1;
                    bestLayer = std::max(bestLayer, candidate);
                }
            }
            layer[nid] = bestLayer;
        }
    }

    struct LayerKey {
        int layer;
        int floor;
        QString type;
        bool operator<(const LayerKey &o) const {
            if (layer != o.layer) return layer < o.layer;
            if (floor != o.floor) return floor < o.floor;
            return type < o.type;
        }
    };

    QMap<LayerKey, QVector<NodeId>> cells;
    int maxLayer = 0;
    for (const auto &[nid, info] : nodeMap) {
        int l = layer.value(nid, 0);
        maxLayer = std::max(maxLayer, l);
        LayerKey key{l, info.floor, QString::fromStdString(info.type)};
        cells[key].append(nid);
    }

    const double xSpacing = 200.0;
    const double ySpacing = 90.0;
    const double floorGap = 40.0;
    const double typeGap = 20.0;

    for (int l = 0; l <= maxLayer; ++l) {
        double x = l * xSpacing;
        double yCursor = 0.0;

        QList<LayerKey> keysInLayer;
        for (auto it = cells.begin(); it != cells.end(); ++it) {
            if (it.key().layer == l)
                keysInLayer.append(it.key());
        }
        std::sort(keysInLayer.begin(), keysInLayer.end());

        int prevFloor = INT_MIN;
        QString prevType;

        for (const auto &key : keysInLayer) {
            QVector<NodeId> &group = cells[key];
            if (group.isEmpty()) continue;

            if (key.floor != prevFloor) {
                if (prevFloor != INT_MIN)
                    yCursor += floorGap;
                prevFloor = key.floor;
                prevType.clear();
            } else if (key.type != prevType) {
                yCursor += typeGap;
            }
            prevType = key.type;

            int n = group.size();
            double totalHeight = (n - 1) * ySpacing;
            double startY = yCursor - totalHeight / 2.0;

            for (int i = 0; i < n; ++i) {
                double y = startY + i * ySpacing;
                QPointF newPos(x, y);
                _model->setNodeData(group[i], NodeRole::Position, newPos);
                if (auto *ngo = _scene->nodeGraphicsObject(group[i]))
                    ngo->setPos(newPos);
            }

            yCursor += totalHeight / 2.0 + ySpacing / 2.0;
        }
    }

    _view->fitInView(_scene->sceneRect(), Qt::KeepAspectRatio);
}