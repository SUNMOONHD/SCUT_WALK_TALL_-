#pragma once

#include "metro_graph.h"

#include <QMatrix4x4>
#include <QMouseEvent>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QVector3D>
#include <QVector>
#include <QWheelEvent>
#include <QString>
#include <unordered_map>

struct Vertex3D {
    QVector3D position;
    QVector3D normal;
    QVector3D color;
};

struct Passenger3DInfo {
    std::string id;
    float x, y, z;
    float progress;  // 0-1, 在边上的进度
    std::string state;
};

class Station3DView : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit Station3DView(QWidget *parent = nullptr);
    ~Station3DView() override;

    void setGraph(const MetroGraph &graph);
    void setPassengers(const std::vector<Passenger3DInfo> &passengers);
    void setVisibleFloor(int floor);
    void setShowAllFloors(bool showAll);
    int getVisibleFloor() const { return visibleFloor_; }
    bool getShowAllFloors() const { return showAllFloors_; }

signals:
    void floorChanged(int floor);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void buildGeometry(bool resetCamera = true);
    void buildSphere(QVector3D center, float radius, QVector3D color, int rings, int sectors);
    void buildCylinder(QVector3D p1, QVector3D p2, float radius, QVector3D color, int sectors);
    void buildFloorPlane(int floor, float floorHeight, float halfExtent);
    void buildGrid(float halfExtent, float y, int divisions);
    void buildPassenger(const Passenger3DInfo &p);
    QVector3D nodeColor(const std::string &type) const;
    float nodeRadius(const std::string &type) const;
    QVector3D passengerColor(const std::string &state) const;
    bool shouldShowNode(int nodeFloor) const;
    QString nodeTypeDisplayName(const std::string &type) const;
    void updateHoverTooltip(const QPoint &pos);

    MetroGraph graph_;
    std::vector<Passenger3DInfo> passengers_;
    bool graphDirty_ = false;
    bool passengersDirty_ = false;

    int visibleFloor_ = 0;
    bool showAllFloors_ = true;
    int minFloor_ = 0;
    int maxFloor_ = 0;

    QOpenGLShaderProgram *program_ = nullptr;
    QOpenGLVertexArrayObject vao_;
    QOpenGLBuffer vbo_;

    QVector<Vertex3D> vertices_;
    QVector<GLuint> indices_;

    QMatrix4x4 projection_;
    QMatrix4x4 view_;
    QMatrix4x4 model_;

    float cameraDistance_ = 300.0f;
    float cameraYaw_ = -35.0f;
    float cameraPitch_ = 35.0f;
    QVector3D cameraTarget_;

    QPoint lastMousePos_;
    bool rotating_ = false;
    bool panning_ = false;
};