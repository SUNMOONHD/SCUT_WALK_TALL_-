#include "station_3d_view.h"

#include <QOpenGLShader>
#include <QPainter>
#include <QSurfaceFormat>
#include <QToolTip>
#include <QtMath>
#include <algorithm>
#include <cmath>

static constexpr float kCoordScale = 8.0f;
static constexpr float kFloorHeight = 60.0f;
static constexpr float kNodeRadius = 5.0f;
static constexpr float kEdgeRadius = 1.2f;
static constexpr int kSphereRings = 16;
static constexpr int kSphereSectors = 24;
static constexpr int kCylSectors = 12;

Station3DView::Station3DView(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setMinimumSize(400, 300);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    QSurfaceFormat fmt = format();
    fmt.setSamples(8);
    fmt.setSwapInterval(1);
    setFormat(fmt);
}

Station3DView::~Station3DView()
{
    makeCurrent();
    vao_.destroy();
    vbo_.destroy();
    delete program_;
    doneCurrent();
}

void Station3DView::setGraph(const MetroGraph &graph)
{
    graph_ = graph;
    graphDirty_ = true;
    
    // 更新楼层范围
    minFloor_ = 0;
    maxFloor_ = 0;
    bool first = true;
    for (const auto &[id, node] : graph_.nodes()) {
        if (first) {
            minFloor_ = maxFloor_ = node.floor;
            first = false;
        } else {
            minFloor_ = std::min(minFloor_, node.floor);
            maxFloor_ = std::max(maxFloor_, node.floor);
        }
    }
    
    update();
}

void Station3DView::setPassengers(const std::vector<Passenger3DInfo> &passengers)
{
    passengers_ = passengers;
    passengersDirty_ = true;
    update();
}

void Station3DView::setVisibleFloor(int floor)
{
    visibleFloor_ = floor;
    showAllFloors_ = false;
    graphDirty_ = true;
    emit floorChanged(floor);
    update();
}

void Station3DView::setShowAllFloors(bool showAll)
{
    showAllFloors_ = showAll;
    graphDirty_ = true;
    update();
}

QVector3D Station3DView::nodeColor(const std::string &type) const
{
    if (type == "entrance")
        return {0.18f, 0.82f, 0.28f};
    if (type == "exit")
        return {0.95f, 0.55f, 0.55f};
    if (type == "platform")
        return {0.22f, 0.55f, 0.91f};
    if (type == "gate")
        return {0.95f, 0.55f, 0.10f};
    if (type == "ticket")
        return {0.85f, 0.25f, 0.25f};
    if (type == "security")
        return {0.55f, 0.55f, 0.58f};
    if (type == "stairs")
        return {0.72f, 0.35f, 0.72f};
    if (type == "escalator")
        return {0.90f, 0.50f, 0.90f};
    if (type == "corridor")
        return {0.35f, 0.68f, 0.68f};
    if (type == "hall")
        return {0.92f, 0.78f, 0.42f};
    if (type == "waiting")
        return {0.45f, 0.65f, 0.88f};
    return {0.60f, 0.60f, 0.60f};
}

float Station3DView::nodeRadius(const std::string &type) const
{
    if (type == "platform" || type == "hall")
        return kNodeRadius * 1.5f;
    if (type == "entrance" || type == "exit")
        return kNodeRadius * 1.2f;
    if (type == "corridor" || type == "waiting")
        return kNodeRadius * 1.0f;
    if (type == "stairs" || type == "escalator")
        return kNodeRadius * 0.8f;
    return kNodeRadius;
}

QVector3D Station3DView::passengerColor(const std::string &state) const
{
    if (state == "Enter")
        return {0.95f, 0.25f, 0.25f};   // 红色 - 进站
    if (state == "Exit")
        return {0.25f, 0.75f, 0.25f};   // 绿色 - 出站
    if (state == "Security")
        return {0.95f, 0.65f, 0.15f};   // 橙色 - 安检
    if (state == "Ticket")
        return {0.65f, 0.25f, 0.85f};   // 紫色 - 购票
    if (state == "Wait")
        return {0.35f, 0.65f, 0.95f};   // 蓝色 - 等待
    if (state == "Boarding")
        return {0.25f, 0.75f, 0.75f};   // 青色 - 乘车
    return {0.85f, 0.85f, 0.85f};       // 灰色 - 默认
}

bool Station3DView::shouldShowNode(int nodeFloor) const
{
    if (showAllFloors_)
        return true;
    return nodeFloor == visibleFloor_;
}

void Station3DView::buildSphere(QVector3D center, float radius, QVector3D color,
                                 int rings, int sectors)
{
    int base = vertices_.size();

    for (int ring = 0; ring <= rings; ++ring) {
        float phi = static_cast<float>(M_PI) * ring / rings;
        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);

        for (int sec = 0; sec <= sectors; ++sec) {
            float theta = 2.0f * static_cast<float>(M_PI) * sec / sectors;
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            QVector3D normal(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta);
            QVector3D pos = center + normal * radius;
            vertices_.append({pos, normal, color});
        }
    }

    int cols = sectors + 1;
    for (int ring = 0; ring < rings; ++ring) {
        for (int sec = 0; sec < sectors; ++sec) {
            GLuint a = base + ring * cols + sec;
            GLuint b = a + cols;
            GLuint c = a + 1;
            GLuint d = b + 1;
            indices_.append(a); indices_.append(b); indices_.append(c);
            indices_.append(c); indices_.append(b); indices_.append(d);
        }
    }
}

void Station3DView::buildCylinder(QVector3D p1, QVector3D p2, float radius,
                                   QVector3D color, int sectors)
{
    QVector3D dir = p2 - p1;
    float length = dir.length();
    if (length < 0.001f)
        return;

    QVector3D axis = dir.normalized();
    QVector3D perp;
    if (std::abs(axis.x()) < 0.9f)
        perp = QVector3D::crossProduct(axis, QVector3D(1, 0, 0)).normalized();
    else
        perp = QVector3D::crossProduct(axis, QVector3D(0, 1, 0)).normalized();
    QVector3D perp2 = QVector3D::crossProduct(axis, perp).normalized();

    int base = vertices_.size();

    for (int i = 0; i <= sectors; ++i) {
        float angle = 2.0f * static_cast<float>(M_PI) * i / sectors;
        float c = std::cos(angle);
        float s = std::sin(angle);
        QVector3D normal = (perp * c + perp2 * s).normalized();
        QVector3D offset = normal * radius;

        vertices_.append({p1 + offset, normal, color});
        vertices_.append({p2 + offset, normal, color});
    }

    for (int i = 0; i < sectors; ++i) {
        GLuint a = base + i * 2;
        GLuint b = a + 1;
        GLuint c = a + 2;
        GLuint d = a + 3;
        indices_.append(a); indices_.append(c); indices_.append(b);
        indices_.append(b); indices_.append(c); indices_.append(d);
    }
}

void Station3DView::buildFloorPlane(int floor, float floorHeight, float halfExtent)
{
    float y = static_cast<float>(floor) * floorHeight;
    QVector3D color(0.28f, 0.28f, 0.30f);
    QVector3D normal(0, 1, 0);

    int base = vertices_.size();
    vertices_.append({{-halfExtent, y, -halfExtent}, normal, color});
    vertices_.append({{ halfExtent, y, -halfExtent}, normal, color});
    vertices_.append({{ halfExtent, y,  halfExtent}, normal, color});
    vertices_.append({{-halfExtent, y,  halfExtent}, normal, color});

    indices_.append(base);     indices_.append(base + 1); indices_.append(base + 2);
    indices_.append(base);     indices_.append(base + 2); indices_.append(base + 3);
}

void Station3DView::buildGrid(float halfExtent, float y, int divisions)
{
    QVector3D color(0.35f, 0.35f, 0.38f);
    QVector3D normal(0, 1, 0);
    float step = 2.0f * halfExtent / divisions;

    int base = vertices_.size();
    for (int i = 0; i <= divisions; ++i) {
        float pos = -halfExtent + i * step;
        vertices_.append({{pos, y, -halfExtent}, normal, color});
        vertices_.append({{pos, y,  halfExtent}, normal, color});
        vertices_.append({{-halfExtent, y, pos}, normal, color});
        vertices_.append({{ halfExtent, y, pos}, normal, color});
    }

    for (int i = 0; i < (divisions + 1) * 4; i += 2) {
        indices_.append(base + i);
        indices_.append(base + i + 1);
    }
}

void Station3DView::buildPassenger(const Passenger3DInfo &p)
{
    QVector3D center(p.x * kCoordScale, p.z, p.y * kCoordScale);
    buildSphere(center, 2.5f, passengerColor(p.state), 8, 12);
}

void Station3DView::buildGeometry(bool resetCamera)
{
    vertices_.clear();
    indices_.clear();

    if (graph_.nodeCount() == 0)
        return;

    float minX = 0, maxX = 0, minZ = 0, maxZ = 0;
    bool first = true;
    int minFloor = minFloor_, maxFloor = maxFloor_;

    for (const auto &[id, node] : graph_.nodes()) {
        if (!shouldShowNode(node.floor))
            continue;
        float sx = node.x * kCoordScale;
        float sz = node.y * kCoordScale;
        if (first) {
            minX = maxX = sx;
            minZ = maxZ = sz;
            first = false;
        } else {
            minX = std::min(minX, sx);
            maxX = std::max(maxX, sx);
            minZ = std::min(minZ, sz);
            maxZ = std::max(maxZ, sz);
        }
    }

    float halfExtent = std::max({std::abs(minX), std::abs(maxX),
                                  std::abs(minZ), std::abs(maxZ), 80.0f}) * 1.4f;

    float bottomY = (minFloor - 0.5f) * kFloorHeight;
    buildGrid(halfExtent, bottomY, 20);

    // 只显示当前楼层的地板（如果不是显示所有楼层）
    if (showAllFloors_) {
        for (int f = minFloor; f <= maxFloor; ++f)
            buildFloorPlane(f, kFloorHeight, halfExtent);
    } else {
        buildFloorPlane(visibleFloor_, kFloorHeight, halfExtent);
    }

    // 只显示当前楼层的节点（如果不是显示所有楼层）
    for (const auto &[id, node] : graph_.nodes()) {
        if (!shouldShowNode(node.floor))
            continue;
        QVector3D center(node.x * kCoordScale,
                         node.floor * kFloorHeight,
                         node.y * kCoordScale);
        buildSphere(center, nodeRadius(node.type), nodeColor(node.type),
                    kSphereRings, kSphereSectors);
    }

    // 只显示当前楼层的边（如果不是显示所有楼层）
    for (const auto &edge : graph_.edges()) {
        auto fromIt = graph_.nodes().find(edge.from);
        auto toIt = graph_.nodes().find(edge.to);
        if (fromIt == graph_.nodes().end() || toIt == graph_.nodes().end())
            continue;

        // 如果不是显示所有楼层，只显示同楼层的边
        if (!showAllFloors_) {
            if (fromIt->second.floor != visibleFloor_ || toIt->second.floor != visibleFloor_)
                continue;
        }

        QVector3D p1(fromIt->second.x * kCoordScale,
                     fromIt->second.floor * kFloorHeight,
                     fromIt->second.y * kCoordScale);
        QVector3D p2(toIt->second.x * kCoordScale,
                     toIt->second.floor * kFloorHeight,
                     toIt->second.y * kCoordScale);

        QVector3D edgeColor(0.50f, 0.50f, 0.55f);
        if (edge.lineIndex == 1)
            edgeColor = {0.85f, 0.35f, 0.30f};
        else if (edge.lineIndex == 2)
            edgeColor = {0.22f, 0.55f, 0.85f};
        else if (edge.lineIndex == 3)
            edgeColor = {0.22f, 0.72f, 0.35f};

        buildCylinder(p1, p2, kEdgeRadius, edgeColor, kCylSectors);
    }

    // 添加乘客
    for (const auto &p : passengers_) {
        if (!showAllFloors_) {
            int passengerFloor = static_cast<int>(p.z / kFloorHeight);
            if (passengerFloor != visibleFloor_)
                continue;
        }
        buildPassenger(p);
    }

    if (resetCamera) {
        cameraTarget_ = QVector3D(0, (minFloor + maxFloor) * 0.5f * kFloorHeight, 0);
        cameraDistance_ = halfExtent * 2.2f;
    }
}

void Station3DView::initializeGL()
{
    initializeOpenGLFunctions();

    glClearColor(0.16f, 0.16f, 0.18f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);

    const char *vertexShader = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec3 aNormal;
        layout(location = 2) in vec3 aColor;

        uniform mat4 uModel;
        uniform mat4 uView;
        uniform mat4 uProjection;
        uniform mat4 uNormalMatrix;

        out vec3 vFragPos;
        out vec3 vNormal;
        out vec3 vColor;

        void main() {
            vec4 worldPos = uModel * vec4(aPos, 1.0);
            vFragPos = worldPos.xyz;
            vNormal = mat3(uNormalMatrix) * aNormal;
            vColor = aColor;
            gl_Position = uProjection * uView * worldPos;
        }
    )";

    const char *fragmentShader = R"(
        #version 330 core
        in vec3 vFragPos;
        in vec3 vNormal;
        in vec3 vColor;

        uniform vec3 uLightPos;
        uniform vec3 uViewPos;
        uniform vec3 uAmbientColor;
        uniform float uAmbientStrength;
        uniform float uSpecularStrength;
        uniform float uShininess;

        out vec4 fragColor;

        void main() {
            vec3 ambient = uAmbientStrength * uAmbientColor;

            vec3 norm = normalize(vNormal);
            vec3 lightDir = normalize(uLightPos - vFragPos);
            float diff = max(dot(norm, lightDir), 0.0);
            vec3 diffuse = diff * vec3(1.0, 1.0, 1.0);

            vec3 viewDir = normalize(uViewPos - vFragPos);
            vec3 reflectDir = reflect(-lightDir, norm);
            float spec = pow(max(dot(viewDir, reflectDir), 0.0), uShininess);
            vec3 specular = uSpecularStrength * spec * vec3(1.0, 1.0, 1.0);

            vec3 result = (ambient + diffuse + specular) * vColor;
            fragColor = vec4(result, 1.0);
        }
    )";

    program_ = new QOpenGLShaderProgram(this);
    program_->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader);
    program_->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShader);
    program_->link();

    vao_.create();
    vbo_.create();
}

void Station3DView::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    projection_.setToIdentity();
    projection_.perspective(45.0f, static_cast<float>(w) / std::max(h, 1), 1.0f, 8000.0f);
}

void Station3DView::paintGL()
{
    bool resetCamera = graphDirty_;
    if (graphDirty_ || passengersDirty_) {
        buildGeometry(resetCamera);
        graphDirty_ = false;
        passengersDirty_ = false;

        vao_.bind();
        vbo_.bind();
        vbo_.allocate(vertices_.constData(),
                      static_cast<int>(vertices_.size() * sizeof(Vertex3D)));
        vbo_.release();
        vao_.release();
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    program_->bind();
    vao_.bind();
    vbo_.bind();

    program_->enableAttributeArray(0);
    program_->enableAttributeArray(1);
    program_->enableAttributeArray(2);
    program_->setAttributeBuffer(0, GL_FLOAT, 0, 3, sizeof(Vertex3D));
    program_->setAttributeBuffer(1, GL_FLOAT, offsetof(Vertex3D, normal), 3, sizeof(Vertex3D));
    program_->setAttributeBuffer(2, GL_FLOAT, offsetof(Vertex3D, color), 3, sizeof(Vertex3D));

    float yawRad = qDegreesToRadians(cameraYaw_);
    float pitchRad = qDegreesToRadians(cameraPitch_);
    QVector3D eye(cameraTarget_.x() + cameraDistance_ * std::cos(pitchRad) * std::sin(yawRad),
                  cameraTarget_.y() + cameraDistance_ * std::sin(pitchRad),
                  cameraTarget_.z() + cameraDistance_ * std::cos(pitchRad) * std::cos(yawRad));

    view_.setToIdentity();
    view_.lookAt(eye, cameraTarget_, QVector3D(0, 1, 0));

    program_->setUniformValue("uModel", model_);
    program_->setUniformValue("uView", view_);
    program_->setUniformValue("uProjection", projection_);

    QMatrix4x4 normalMatrix = (view_ * model_).inverted().transposed();
    program_->setUniformValue("uNormalMatrix", normalMatrix);

    program_->setUniformValue("uLightPos", eye + QVector3D(100, 200, 100));
    program_->setUniformValue("uViewPos", eye);
    program_->setUniformValue("uAmbientColor", QVector3D(1.0f, 1.0f, 1.0f));
    program_->setUniformValue("uAmbientStrength", 0.35f);
    program_->setUniformValue("uSpecularStrength", 0.5f);
    program_->setUniformValue("uShininess", 32.0f);

    glDrawElements(GL_TRIANGLES, indices_.size(), GL_UNSIGNED_INT, indices_.constData());

    vbo_.release();
    vao_.release();
    program_->release();

    // 2D text label overlay
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QFont labelFont;
    labelFont.setPixelSize(12);
    labelFont.setBold(true);
    painter.setFont(labelFont);

    QMatrix4x4 mvp = projection_ * view_ * model_;
    float w = static_cast<float>(width());
    float h = static_cast<float>(height());

    for (const auto &[id, node] : graph_.nodes()) {
        if (!shouldShowNode(node.floor))
            continue;

        QVector3D world(node.x * kCoordScale,
                        node.floor * kFloorHeight,
                        node.y * kCoordScale);
        QVector3D clip = mvp.map(world);

        if (clip.z() < -1.0f || clip.z() > 1.0f)
            continue;

        float sx = (clip.x() * 0.5f + 0.5f) * w;
        float sy = (1.0f - (clip.y() * 0.5f + 0.5f)) * h;

        QString label = node.name.empty()
            ? nodeTypeDisplayName(node.type)
            : QString::fromStdString(node.name);

        QVector3D color = nodeColor(node.type);
        QColor textColor(static_cast<int>(color.x() * 255),
                         static_cast<int>(color.y() * 255),
                         static_cast<int>(color.z() * 255));

        painter.setPen(Qt::black);
        painter.drawText(QPointF(sx + 1, sy - 7), label);
        painter.setPen(textColor);
        painter.drawText(QPointF(sx, sy - 8), label);
    }

    painter.end();
}

void Station3DView::mousePressEvent(QMouseEvent *event)
{
    lastMousePos_ = event->pos();
    if (event->button() == Qt::LeftButton)
        rotating_ = true;
    else if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton)
        panning_ = true;
}

void Station3DView::mouseMoveEvent(QMouseEvent *event)
{
    QPoint delta = event->pos() - lastMousePos_;
    lastMousePos_ = event->pos();

    if (rotating_) {
        cameraYaw_ += delta.x() * 0.4f;
        cameraPitch_ += delta.y() * 0.4f;
        cameraPitch_ = qBound(-89.0f, cameraPitch_, 89.0f);
        update();
    } else if (panning_) {
        float scale = cameraDistance_ * 0.0015f;
        float yawRad = qDegreesToRadians(cameraYaw_);
        QVector3D right(std::cos(yawRad), 0, -std::sin(yawRad));
        QVector3D up(0, 1, 0);
        cameraTarget_ += right * (-delta.x() * scale) + up * (delta.y() * scale);
        update();
    } else {
        updateHoverTooltip(event->pos());
    }
}

void Station3DView::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    rotating_ = false;
    panning_ = false;
}

void Station3DView::wheelEvent(QWheelEvent *event)
{
    cameraDistance_ *= (event->angleDelta().y() > 0) ? 0.92f : 1.08f;
    cameraDistance_ = qBound(20.0f, cameraDistance_, 5000.0f);
    update();
}

QString Station3DView::nodeTypeDisplayName(const std::string &type) const
{
    if (type == "entrance")  return QStringLiteral("入口");
    if (type == "exit")      return QStringLiteral("出口");
    if (type == "platform")  return QStringLiteral("站台");
    if (type == "corridor")  return QStringLiteral("通道");
    if (type == "stairs")    return QStringLiteral("楼梯");
    if (type == "escalator") return QStringLiteral("扶梯");
    if (type == "gate")      return QStringLiteral("闸机");
    if (type == "security")  return QStringLiteral("安检区");
    if (type == "ticket")    return QStringLiteral("售票区");
    if (type == "hall")      return QStringLiteral("大厅");
    if (type == "waiting")   return QStringLiteral("候车区");
    return QString::fromStdString(type);
}

void Station3DView::updateHoverTooltip(const QPoint &pos)
{
    float w = static_cast<float>(width());
    float h = static_cast<float>(height());
    if (w < 1 || h < 1) return;

    QMatrix4x4 mvp = projection_ * view_ * model_;

    float bestDist = 30.0f;
    QString bestTip;

    for (const auto &[id, node] : graph_.nodes()) {
        if (!shouldShowNode(node.floor)) continue;

        QVector3D world(node.x * kCoordScale,
                        node.floor * kFloorHeight,
                        node.y * kCoordScale);
        QVector3D clip = mvp.map(world);

        float sx = (clip.x() * 0.5f + 0.5f) * w;
        float sy = (1.0f - (clip.y() * 0.5f + 0.5f)) * h;

        if (clip.z() < -1.0f || clip.z() > 1.0f) continue;

        float dx = sx - pos.x();
        float dy = sy - pos.y();
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < bestDist) {
            bestDist = dist;
            QString name = node.name.empty()
                ? QString::fromStdString(node.id)
                : QString::fromStdString(node.name);
            bestTip = QStringLiteral("%1 [%2]").arg(name, nodeTypeDisplayName(node.type));
        }
    }

    if (!bestTip.isEmpty()) {
        QToolTip::showText(mapToGlobal(pos), bestTip, this);
    } else {
        QToolTip::hideText();
    }
}