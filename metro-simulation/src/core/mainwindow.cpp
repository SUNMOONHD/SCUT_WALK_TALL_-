#include "mainwindow.h"

#include "asset_catalog.h"
#include "report_writer.h"
#include "result_export.h"
#include "station_editor.h"
#include "station_3d_view.h"
#include "visualization.h"

#include "waitingspinnerwidget.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSvgRenderer>
#include <QStatusBar>
#include <QTableWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace {

QString prettyPath(const QString &path)
{
	return QDir::cleanPath(path);
}

QPixmap loadPixmapWithSvgSupport(const QString &filePath, int maxW, int maxH)
{
	if (filePath.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
		QSvgRenderer renderer(filePath);
		if (!renderer.isValid()) {
			return QPixmap();
		}
		QSize size = renderer.defaultSize();
		if (!size.isValid()) {
			size = QSize(maxW, maxH);
		}
		size.scale(maxW, maxH, Qt::KeepAspectRatio);
		QPixmap pixmap(size);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		renderer.render(&painter);
		return pixmap;
	}

	QPixmap pixmap(filePath);
	if (!pixmap.isNull()) {
		return pixmap.scaled(maxW, maxH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	}
	return QPixmap();
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
{
	setWindowTitle(QStringLiteral("地铁综合换乘站"));
	setWindowIcon(QIcon(resourcePath(QStringLiteral("ui/app_icon.png"))));
	resize(1580, 920);
	setUnifiedTitleAndToolBarOnMac(true);

	buildUi();
	setupToolbar();
	setupConnections();
	loadInitialScenario();
	updateToolbarState();
}

QString MainWindow::resourcePath(const QString &relativePath) const
{
	const QString appDir = QCoreApplication::applicationDirPath();
	const QStringList candidates = {
		QDir(appDir).absoluteFilePath(QStringLiteral("resources/") + relativePath),
		QDir(appDir).absoluteFilePath(QStringLiteral("../resources/") + relativePath),
		QDir(appDir).absoluteFilePath(QStringLiteral("../../resources/") + relativePath),
		QDir::current().absoluteFilePath(QStringLiteral("resources/") + relativePath),
		QDir::current().absoluteFilePath(relativePath)
	};
	for (const QString &candidate : candidates) {
		if (QFileInfo::exists(candidate)) {
			return candidate;
		}
	}
	return candidates.front();
}

QString MainWindow::dataPath(const QString &relativePath) const
{
	const QString appDir = QCoreApplication::applicationDirPath();
	const QStringList candidates = {
		QDir(appDir).absoluteFilePath(relativePath),
		QDir(appDir).absoluteFilePath(QStringLiteral("../") + relativePath),
		QDir(appDir).absoluteFilePath(QStringLiteral("../../") + relativePath),
		QDir::current().absoluteFilePath(relativePath),
		QDir::current().absoluteFilePath(QStringLiteral("bin/") + relativePath)
	};
	for (const QString &candidate : candidates) {
		if (QFileInfo::exists(candidate)) {
			return candidate;
		}
	}
	return candidates.front();
}

QIcon MainWindow::iconForResource(const QString &fileName) const
{
	const QString path = resourcePath(QStringLiteral("icons/") + fileName);
	if (fileName.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
		QSvgRenderer renderer(path);
		if (renderer.isValid()) {
			QSize size = renderer.defaultSize();
			if (!size.isValid()) size = QSize(32, 32);
			size.scale(32, 32, Qt::KeepAspectRatio);
			QPixmap pix(size);
			pix.fill(Qt::transparent);
			QPainter painter(&pix);
			renderer.render(&painter);
			return QIcon(pix);
		}
	}
	return QIcon(path);
}

void MainWindow::buildUi()
{
	auto *central = new QWidget(this);
	auto *layout = new QVBoxLayout(central);
	layout->setContentsMargins(14, 14, 14, 14);
	layout->setSpacing(10);

	dashboard_ = new VisualizationWidget(central);
	layout->addWidget(dashboard_, 1);
	connect(dashboard_, &VisualizationWidget::legendRequested, this, &MainWindow::showLegend);
	setCentralWidget(central);

	statusLabel_ = new QLabel(QStringLiteral("正在加载场景..."), this);
	statusBar()->addPermanentWidget(statusLabel_, 1);

	spinner_ = new WaitingSpinnerWidget(Qt::ApplicationModal, this, true, false);
	spinner_->setColor(QColor(196, 61, 61));
	spinner_->setNumberOfLines(12);
	spinner_->setLineLength(10);
	spinner_->setLineWidth(3);
	spinner_->setInnerRadius(12);
	spinner_->setRevolutionsPerSecond(1.0);

	timer_ = new QTimer(this);
	timer_->setInterval(1000);

	// speed combo
	speedCombo_ = new QComboBox(this);
	speedCombo_->addItem(QStringLiteral("0.25x"), 0.25);
	speedCombo_->addItem(QStringLiteral("0.5x"), 0.5);
	speedCombo_->addItem(QStringLiteral("1x"), 1.0);
	speedCombo_->addItem(QStringLiteral("2x"), 2.0);
	speedCombo_->addItem(QStringLiteral("4x"), 4.0);
	speedCombo_->addItem(QStringLiteral("8x"), 8.0);
	speedCombo_->addItem(QStringLiteral("16x"), 16.0);
	speedCombo_->setCurrentIndex(2);
	connect(speedCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int){
		double factor = speedCombo_->currentData().toDouble();
		int interval = static_cast<int>(1000.0 / factor);
		if (interval < 10) interval = 10;
		timer_->setInterval(interval);
	});

	pathPreferenceCombo_ = new QComboBox(this);
	pathPreferenceCombo_->addItem(QStringLiteral("综合均衡"), static_cast<int>(PathObjective::WeightedSum));
	pathPreferenceCombo_->addItem(QStringLiteral("时间最优"), static_cast<int>(PathObjective::MinTime));
	pathPreferenceCombo_->addItem(QStringLiteral("距离最短"), static_cast<int>(PathObjective::MinDistance));
	pathPreferenceCombo_->addItem(QStringLiteral("避开拥挤"), static_cast<int>(PathObjective::MinCongestion));
	pathPreferenceCombo_->addItem(QStringLiteral("减少换区"), static_cast<int>(PathObjective::MinZoneSwitches));
	pathPreferenceCombo_->setCurrentIndex(0);
	connect(pathPreferenceCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::applyPathPreference);

	useAStarCheck_ = new QCheckBox(QStringLiteral("A* 算法"), this);
	useAStarCheck_->setChecked(true);
	useAStarCheck_->setToolTip(QStringLiteral("启用A*启发式搜索，取消则使用Dijkstra"));
	connect(useAStarCheck_, &QCheckBox::toggled, this, &MainWindow::applyAlgorithmSettings);

	useCacheCheck_ = new QCheckBox(QStringLiteral("路径缓存"), this);
	useCacheCheck_->setChecked(true);
	useCacheCheck_->setToolTip(QStringLiteral("启用路径缓存以提升性能"));
	connect(useCacheCheck_, &QCheckBox::toggled, this, &MainWindow::applyAlgorithmSettings);
}

void MainWindow::setupToolbar()
{
	auto *toolbar = addToolBar(QStringLiteral("仿真控制"));
	mainToolbar_ = toolbar;
	toolbar->setMovable(false);
	toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

	startAction_ = toolbar->addAction(iconForResource(QStringLiteral("btn_start.svg")), QStringLiteral("开始"));
	pauseAction_ = toolbar->addAction(iconForResource(QStringLiteral("btn_pause.svg")), QStringLiteral("暂停"));
	resetAction_ = toolbar->addAction(iconForResource(QStringLiteral("btn_reset.svg")), QStringLiteral("重置"));
	exportAction_ = toolbar->addAction(iconForResource(QStringLiteral("btn_export.svg")), QStringLiteral("导出"));
	startAction_->setToolTip(QStringLiteral("开始仿真"));
	pauseAction_->setToolTip(QStringLiteral("暂停仿真"));
	resetAction_->setToolTip(QStringLiteral("重置为初始状态"));
	exportAction_->setToolTip(QStringLiteral("导出结果与报表到指定目录"));

	toolbar->addSeparator();
	toolbar->addWidget(speedCombo_);

	toolbar->addSeparator();
	diagramAction_ = toolbar->addAction(iconForResource(QStringLiteral("trajectory_log.svg")), QStringLiteral("流程图"));
	diagramAction_->setToolTip(QStringLiteral("查看流程图"));

	toolbar->addSeparator();
	auto *pathLabel = new QLabel(QStringLiteral("路径偏好:"), this);
	pathLabel->setStyleSheet(QStringLiteral("color:#8B7D6B; font-size:12px; margin-left:6px;"));
	toolbar->addWidget(pathLabel);
	toolbar->addWidget(pathPreferenceCombo_);

	toolbar->addSeparator();
	toolbar->addWidget(useAStarCheck_);
	toolbar->addWidget(useCacheCheck_);

	toolbar->addSeparator();
	editAction_ = toolbar->addAction(iconForResource(QStringLiteral("node_topology.svg")), QStringLiteral("编辑拓扑"));
	editAction_->setToolTip(QStringLiteral("打开拓扑编辑器，拖拽修改站点布局"));

	view3dAction_ = toolbar->addAction(iconForResource(QStringLiteral("node_hall.svg")), QStringLiteral("3D 查看"));
	view3dAction_->setToolTip(QStringLiteral("以三维视角查看车站拓扑结构"));

	legendAction_ = toolbar->addAction(iconForResource(QStringLiteral("node_platform.svg")), QStringLiteral("图例"));
	legendAction_->setToolTip(QStringLiteral("查看节点图标与类型对照"));

	pathCompareAction_ = toolbar->addAction(iconForResource(QStringLiteral("trajectory_start.svg")), QStringLiteral("路径对比"));
	pathCompareAction_->setToolTip(QStringLiteral("对比不同策略下的最优路径"));
}

void MainWindow::setupConnections()
{
	connect(startAction_, &QAction::triggered, this, &MainWindow::toggleSimulation);
	connect(pauseAction_, &QAction::triggered, this, &MainWindow::toggleSimulation);
	connect(resetAction_, &QAction::triggered, this, &MainWindow::resetSimulation);
	connect(exportAction_, &QAction::triggered, this, &MainWindow::exportResults);
	connect(diagramAction_, &QAction::triggered, this, &MainWindow::showDiagramImage);
	connect(editAction_, &QAction::triggered, this, &MainWindow::openTopologyEditor);
	connect(view3dAction_, &QAction::triggered, this, &MainWindow::open3DView);
	connect(legendAction_, &QAction::triggered, this, &MainWindow::showLegend);
	connect(pathCompareAction_, &QAction::triggered, this, &MainWindow::openPathCompare);
	connect(timer_, &QTimer::timeout, this, &MainWindow::stepSimulation);
}

void MainWindow::loadInitialScenario()
{
	spinner_->start();
	qApp->processEvents();

	std::string error;
	const QString stationFile = dataPath(QStringLiteral("data/stations/sample_station.json"));
	const QString paramsFile = dataPath(QStringLiteral("data/params/default_params.json"));
	if (!simulation_.loadScenario(stationFile.toStdString(), paramsFile.toStdString(), &error)) {
		spinner_->stop();
		QMessageBox::critical(this, QStringLiteral("场景加载失败"), QString::fromStdString(error));
		statusLabel_->setText(QStringLiteral("场景加载失败"));
		return;
	}

	dashboard_->setGraph(simulation_.graph());
	dashboard_->setSimulation(simulation_);
	statusLabel_->setText(QStringLiteral("场景已加载：%1").arg(QString::fromStdString(simulation_.graph().stationName())));
	spinner_->stop();
}

void MainWindow::stepSimulation()
{
	simulation_.step();
	dashboard_->setSimulation(simulation_);
	statusLabel_->setText(QStringLiteral("运行中... t=%1秒，活跃=%2，完成=%3")
							.arg(simulation_.currentTime())
							.arg(static_cast<int>(simulation_.passengers().size()))
							.arg(simulation_.statistics().completedPassengers()));

	if (view3dWidget_) {
		std::vector<Passenger3DInfo> pInfos;
		const auto &nodes = simulation_.graph().nodes();
		for (const auto &p : simulation_.passengers()) {
			if (p.state == PassengerState::Finished) continue;

			Passenger3DInfo info;
			info.id = std::to_string(p.id);

			if (p.onEdge) {
				auto fromIt = nodes.find(p.edgeFrom);
				auto toIt = nodes.find(p.edgeTo);
				if (fromIt != nodes.end() && toIt != nodes.end()) {
					double t = (p.edgeTravelTotal > 0.0)
						? 1.0 - p.edgeTravelRemaining / p.edgeTravelTotal
						: 0.5;
					info.x = static_cast<float>(fromIt->second.x + (toIt->second.x - fromIt->second.x) * t);
					info.y = static_cast<float>(fromIt->second.y + (toIt->second.y - fromIt->second.y) * t);
					double fromZ = fromIt->second.floor * 60.0;
					double toZ = toIt->second.floor * 60.0;
					info.z = static_cast<float>(fromZ + (toZ - fromZ) * t);
					info.progress = static_cast<float>(t);
				} else {
					continue;
				}
			} else {
				auto nodeIt = nodes.find(p.currentNode);
				if (nodeIt != nodes.end()) {
					info.x = static_cast<float>(nodeIt->second.x);
					info.y = static_cast<float>(nodeIt->second.y);
					info.z = static_cast<float>(nodeIt->second.floor * 60.0);
					info.progress = 0.0f;
				} else {
					continue;
				}
			}

			switch (p.state) {
				case PassengerState::Enter:    info.state = "Enter"; break;
				case PassengerState::Security: info.state = "Security"; break;
				case PassengerState::Ticket:   info.state = "Ticket"; break;
				case PassengerState::Wait:     info.state = "Wait"; break;
				case PassengerState::Board:    info.state = "Boarding"; break;
				case PassengerState::Exit:     info.state = "Exit"; break;
				default:                       info.state = "Enter"; break;
			}

			pInfos.push_back(std::move(info));
		}
		view3dWidget_->setPassengers(pInfos);
	}
}

void MainWindow::refreshDashboard()
{
	dashboard_->setGraph(simulation_.graph());
	dashboard_->setSimulation(simulation_);
}

void MainWindow::toggleSimulation()
{
	running_ = !running_;
	if (running_) {
		timer_->start();
		statusLabel_->setText(QStringLiteral("仿真运行中"));
	} else {
		timer_->stop();
		statusLabel_->setText(QStringLiteral("仿真已暂停"));
	}
	updateToolbarState();
}

void MainWindow::resetSimulation()
{
	timer_->stop();
	running_ = false;
	simulation_.reset();
	dashboard_->clearHistory();
	refreshDashboard();
	statusLabel_->setText(QStringLiteral("仿真已重置"));
	updateToolbarState();
}

void MainWindow::exportResults()
{
	spinner_->start();
	qApp->processEvents();
	QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择导出目录"), QStringLiteral("data/results"));
	if (dir.isEmpty()) {
		spinner_->stop();
		return;
	}

	const QString summaryPath = QDir(dir).absoluteFilePath(QStringLiteral("gui_summary.json"));
	const QString eventsPath = QDir(dir).absoluteFilePath(QStringLiteral("gui_events.csv"));
	const QString reportPath = QDir(dir).absoluteFilePath(QStringLiteral("gui_report.html"));

	const results::ExportPaths paths{
		summaryPath.toStdString(),
		eventsPath.toStdString()
	};
	std::string exportError;
	if (!results::exportStep3Results(simulation_, paths, &exportError)) {
		spinner_->stop();
		QMessageBox::warning(this, QStringLiteral("导出失败"), QString::fromStdString(exportError));
		return;
	}

	if (!reports::writeStep3HtmlReport(simulation_, reportPath.toStdString(), &exportError)) {
		spinner_->stop();
		QMessageBox::warning(this, QStringLiteral("报表生成失败"), QString::fromStdString(exportError));
		return;
	}

	spinner_->stop();
	QMessageBox::information(this, QStringLiteral("导出完成"),
		QStringLiteral("文件已保存到：\n%1\n%2\n%3")
			.arg(summaryPath)
			.arg(eventsPath)
			.arg(reportPath));
}

void MainWindow::applyPathPreference(int index)
{
	if (!pathPreferenceCombo_) return;
	auto config = simulation_.config();
	config.pathObjective = static_cast<PathObjective>(pathPreferenceCombo_->itemData(index).toInt());

	switch (config.pathObjective) {
		case PathObjective::MinTime:
			config.pathWeights = {1.0, 0.0, 0.0, 0.0};
			break;
		case PathObjective::MinDistance:
			config.pathWeights = {0.0, 1.0, 0.0, 0.0};
			break;
		case PathObjective::MinCongestion:
			config.pathWeights = {0.0, 0.0, 1.0, 0.0};
			break;
		case PathObjective::MinZoneSwitches:
			config.pathWeights = {0.0, 0.0, 0.0, 1.0};
			break;
		case PathObjective::WeightedSum:
		default:
			config.pathWeights = {0.4, 0.2, 0.3, 0.1};
			break;
	}
	simulation_.setConfig(config);
	statusLabel_->setText(QStringLiteral("路径偏好已更新: %1").arg(pathPreferenceCombo_->currentText()));
}

void MainWindow::applyAlgorithmSettings()
{
	if (!useAStarCheck_ || !useCacheCheck_) return;
	auto config = simulation_.config();
	config.useAStar = useAStarCheck_->isChecked();
	config.usePathCache = useCacheCheck_->isChecked();
	simulation_.setConfig(config);
	QStringList parts;
	if (config.useAStar) parts.append(QStringLiteral("A*")); else parts.append(QStringLiteral("Dijkstra"));
	if (config.usePathCache) parts.append(QStringLiteral("缓存")); else parts.append(QStringLiteral("无缓存"));
	statusLabel_->setText(QStringLiteral("算法设置已更新: %1").arg(parts.join(QStringLiteral("+"))));
}

void MainWindow::updateToolbarState()
{
	if (startAction_ && pauseAction_) {
		startAction_->setVisible(!running_);
		pauseAction_->setVisible(running_);
	}
}

void MainWindow::showDiagramImage()
{
	showImageDialog(
		QStringLiteral("流程图预览"),
		{QStringLiteral("diagrams/passenger_fsm_flow.svg"), QStringLiteral("diagrams/passenger_fsm_flow.png")});
}

void MainWindow::showImageDialog(const QString &title, const QStringList &relativeCandidates)
{
	QString imagePath;
	for (const QString &candidate : relativeCandidates) {
		const QString resolved = resourcePath(candidate);
		if (QFileInfo::exists(resolved)) {
			imagePath = resolved;
			break;
		}
	}

	if (imagePath.isEmpty()) {
		QMessageBox::warning(this, QStringLiteral("图片不存在"), QStringLiteral("未找到可预览图片：%1").arg(relativeCandidates.join(QStringLiteral(" / "))));
		return;
	}

	auto *dialog = new QDialog(this);
	dialog->setWindowTitle(title);
	dialog->resize(980, 700);
	dialog->setWindowIcon(QIcon(resourcePath(QStringLiteral("ui/app_icon.png"))));

	auto *layout = new QVBoxLayout(dialog);
	layout->setContentsMargins(12, 12, 12, 12);

	auto *hint = new QLabel(QStringLiteral("当前文件：%1").arg(prettyPath(imagePath)), dialog);
	hint->setWordWrap(true);
	hint->setStyleSheet(QStringLiteral("color:#8fa3c6;"));
	layout->addWidget(hint);

	auto *toolbar = new QHBoxLayout();
	auto *zoomInBtn = new QPushButton(QStringLiteral("放大 +"), dialog);
	auto *zoomOutBtn = new QPushButton(QStringLiteral("缩小 -"), dialog);
	auto *zoomFitBtn = new QPushButton(QStringLiteral("适应窗口"), dialog);
	auto *zoomLabel = new QLabel(QStringLiteral("100%"), dialog);
	const QString btnStyle = QStringLiteral(
		"QPushButton{background:#E8DDD0;color:#3D322C;border:1px solid #C4A882;"
		"border-radius:4px;padding:4px 12px;font-size:12px;}"
		"QPushButton:hover{background:#D4C5B2;}");
	zoomInBtn->setStyleSheet(btnStyle);
	zoomOutBtn->setStyleSheet(btnStyle);
	zoomFitBtn->setStyleSheet(btnStyle);
	zoomLabel->setStyleSheet(QStringLiteral("color:#3D322C;font-size:12px;font-weight:bold;"));
	toolbar->addWidget(zoomInBtn);
	toolbar->addWidget(zoomOutBtn);
	toolbar->addWidget(zoomFitBtn);
	toolbar->addWidget(zoomLabel);
	toolbar->addStretch(1);
	layout->addLayout(toolbar);

	auto *scroll = new QScrollArea(dialog);
	scroll->setWidgetResizable(false);
	auto *imageLabel = new QLabel(scroll);
	imageLabel->setAlignment(Qt::AlignCenter);

	const QPixmap pixmap = loadPixmapWithSvgSupport(imagePath, 1800, 1200);
	if (pixmap.isNull()) {
		imageLabel->setText(QStringLiteral("图片加载失败：%1").arg(prettyPath(imagePath)));
		imageLabel->setStyleSheet(QStringLiteral("color:#d97d7d;"));
	} else {
		imageLabel->setPixmap(pixmap);
		imageLabel->resize(pixmap.size());
	}

	scroll->setWidget(imageLabel);
	layout->addWidget(scroll, 1);

	auto *scaleFactor = new double(1.0);

	auto applyZoom = [imageLabel, zoomLabel, scaleFactor, &pixmap]() {
		int w = static_cast<int>(pixmap.width() * (*scaleFactor));
		int h = static_cast<int>(pixmap.height() * (*scaleFactor));
		imageLabel->setPixmap(pixmap.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation));
		imageLabel->resize(w, h);
		zoomLabel->setText(QStringLiteral("%1%").arg(static_cast<int>(*scaleFactor * 100)));
	};

	connect(zoomInBtn, &QPushButton::clicked, dialog, [scaleFactor, applyZoom]() {
		*scaleFactor = std::min(5.0, *scaleFactor * 1.25);
		applyZoom();
	});
	connect(zoomOutBtn, &QPushButton::clicked, dialog, [scaleFactor, applyZoom]() {
		*scaleFactor = std::max(0.1, *scaleFactor / 1.25);
		applyZoom();
	});
	connect(zoomFitBtn, &QPushButton::clicked, dialog, [scaleFactor, applyZoom, scroll, &pixmap]() {
		double fw = static_cast<double>(scroll->viewport()->width()) / pixmap.width();
		double fh = static_cast<double>(scroll->viewport()->height()) / pixmap.height();
		*scaleFactor = std::min(fw, fh);
		applyZoom();
	});

	scroll->installEventFilter(dialog);
	dialog->exec();
	delete scaleFactor;
}

void MainWindow::showLegend()
{
	QDialog dlg(this);
	dlg.setWindowTitle(QStringLiteral("节点图例"));
	dlg.setMinimumSize(360, 420);
	dlg.setStyleSheet(QStringLiteral(
		"QDialog{background:#FEFAF3;}"
		"QLabel{color:#3D322C;}"));

	auto *mainLayout = new QVBoxLayout(&dlg);
	mainLayout->setSpacing(8);
	mainLayout->setContentsMargins(16, 16, 16, 16);

	auto *titleLabel = new QLabel(QStringLiteral("节点图标对照表"), &dlg);
	titleLabel->setStyleSheet(QStringLiteral("font-size:16px;font-weight:bold;color:#3D322C;"));
	mainLayout->addWidget(titleLabel);

	auto *scrollArea = new QScrollArea(&dlg);
	scrollArea->setWidgetResizable(true);
	scrollArea->setStyleSheet(QStringLiteral("QScrollArea{border:1px solid #D4C5B2;border-radius:8px;background:#FEFAF3;}"));

	auto *scrollWidget = new QWidget();
	auto *grid = new QGridLayout(scrollWidget);
	grid->setSpacing(6);
	grid->setContentsMargins(12, 12, 12, 12);

	struct LegendEntry {
		QString iconFile;
		QString typeKey;
		QString label;
		QString description;
	};
	const QList<LegendEntry> entries = {
		{"node_entrance.svg",  "entrance",  QStringLiteral("入口"),   QStringLiteral("乘客进站入口，客流由此进入车站")},
		{"node_exit.svg",      "exit",      QStringLiteral("出口"),   QStringLiteral("乘客出站出口，完成行程后离开")},
		{"node_platform.svg",  "platform",  QStringLiteral("站台"),   QStringLiteral("候车与上下车区域，连接多条线路")},
		{"node_corridor.svg",  "corridor",  QStringLiteral("通道"),   QStringLiteral("连接各功能区的通行走廊")},
		{"node_stairs.svg",    "stairs",    QStringLiteral("楼梯"),   QStringLiteral("步行楼梯，连接不同楼层")},
		{"node_escalator.svg", "escalator", QStringLiteral("扶梯"),   QStringLiteral("自动扶梯，快速跨层通行")},
		{"node_gate.svg",      "gate",      QStringLiteral("闸机"),   QStringLiteral("检票闸机，控制进出站客流")},
		{"node_security.svg",  "security",  QStringLiteral("安检"),   QStringLiteral("安全检查区域，乘客必经环节")},
		{"node_ticket.svg",    "ticket",    QStringLiteral("售票"),   QStringLiteral("售票窗口或自助售票机")},
		{"node_hall.svg",      "hall",      QStringLiteral("大厅"),   QStringLiteral("站厅层，客流集散与分流区域")},
		{"node_waiting.svg",   "waiting",   QStringLiteral("候车区"), QStringLiteral("乘客候车等待区域")},
	};

	for (int i = 0; i < entries.size(); ++i) {
		const auto &entry = entries[i];
		const int row = i / 2;
		const int col = (i % 2) * 2;

		auto *iconLabel = new QLabel();
		const QString iconPath = resourcePath("icons/" + entry.iconFile);
		QPixmap pix(iconPath);
		if (!pix.isNull()) {
			iconLabel->setPixmap(pix.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
		}
		iconLabel->setFixedSize(36, 36);
		iconLabel->setAlignment(Qt::AlignCenter);
		iconLabel->setToolTip(entry.description);
		grid->addWidget(iconLabel, row, col);

		auto *nameLabel = new QLabel(entry.label);
		nameLabel->setStyleSheet(QStringLiteral("font-size:13px;font-weight:500;"));
		nameLabel->setToolTip(entry.description);
		grid->addWidget(nameLabel, row, col + 1);
	}

	scrollWidget->setLayout(grid);
	scrollArea->setWidget(scrollWidget);
	mainLayout->addWidget(scrollArea);

	auto *closeBtn = new QPushButton(QStringLiteral("关闭"), &dlg);
	closeBtn->setStyleSheet(QStringLiteral(
		"QPushButton{background:#C43D3D;color:white;border:none;border-radius:6px;"
		"padding:8px 24px;font-weight:bold;}"
		"QPushButton:hover{background:#A83232;}"));
	connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
	mainLayout->addWidget(closeBtn, 0, Qt::AlignRight);

	dlg.exec();
}

void MainWindow::openTopologyEditor()
{
	MetroGraph editableGraph = simulation_.graph();

	StationEditorWidget editor(editableGraph, this);
	if (editor.exec() == QDialog::Accepted) {
		simulation_.setGraph(editableGraph);
		refreshDashboard();
		statusLabel_->setText(QStringLiteral("拓扑已更新：%1").arg(QString::fromStdString(editableGraph.stationName())));
	}
}

void MainWindow::openPathCompare()
{
	const auto &nodes = simulation_.graph().nodes();
	if (nodes.size() < 2) {
		QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("当前图中节点不足，无法进行路径对比。"));
		return;
	}

	auto *dlg = new QDialog(this);
	dlg->setWindowTitle(QStringLiteral("路径规划对比"));
	dlg->resize(780, 560);
	dlg->setStyleSheet(QStringLiteral(
		"QDialog{background:#FEFAF3;}"
		"QLabel{color:#3D322C;}"
		"QComboBox{background:#FEFAF3;color:#3D322C;border:1px solid #D4C5B2;border-radius:6px;padding:4px 10px;}"
		"QComboBox:hover{border-color:#C43D3D;}"
		"QPushButton{background:#C43D3D;color:#FEFAF3;border:none;border-radius:8px;padding:8px 16px;font-weight:600;}"
		"QPushButton:hover{background:#A83232;}"
		"QTableWidget{background:#FDF8F0;border:1px solid #D4C5B2;border-radius:6px;font-size:11px;color:#3D322C;gridline-color:#E8DDD0;}"
		"QHeaderView::section{background:#EDE4D8;color:#3D322C;border:none;padding:4px;font-size:11px;font-weight:bold;}"
		"QTableWidget::item:alternate{background:#F5EDE0;}"
		"QTableWidget::item:selected{background:#D4953A;color:white;}"));

	auto *mainLayout = new QVBoxLayout(dlg);
	mainLayout->setSpacing(12);
	mainLayout->setContentsMargins(16, 16, 16, 16);

	auto *titleLabel = new QLabel(QStringLiteral("路径规划对比"), dlg);
	titleLabel->setStyleSheet(QStringLiteral("font-size:18px;font-weight:bold;color:#3D322C;"));
	mainLayout->addWidget(titleLabel);

	auto *selectRow = new QHBoxLayout();
	selectRow->setSpacing(12);

	auto *fromLabel = new QLabel(QStringLiteral("起点:"), dlg);
	auto *fromCombo = new QComboBox(dlg);
	fromCombo->setMinimumWidth(180);
	auto *toLabel = new QLabel(QStringLiteral("终点:"), dlg);
	auto *toCombo = new QComboBox(dlg);
	toCombo->setMinimumWidth(180);

	std::vector<std::pair<std::string, QString>> sortedNodes;
	for (const auto &entry : nodes) {
		QString display = QStringLiteral("%1 (%2)")
			.arg(QString::fromStdString(entry.second.name))
			.arg(QString::fromStdString(entry.second.type));
		sortedNodes.push_back({entry.first, display});
	}
	std::sort(sortedNodes.begin(), sortedNodes.end(),
		[](const auto &a, const auto &b) { return a.second < b.second; });

	for (const auto &item : sortedNodes) {
		fromCombo->addItem(item.second, QString::fromStdString(item.first));
		toCombo->addItem(item.second, QString::fromStdString(item.first));
	}
	if (toCombo->count() > 1) toCombo->setCurrentIndex(1);

	auto *calcBtn = new QPushButton(QStringLiteral("计算路径"), dlg);
	calcBtn->setFixedHeight(32);

	selectRow->addWidget(fromLabel);
	selectRow->addWidget(fromCombo, 1);
	selectRow->addWidget(toLabel);
	selectRow->addWidget(toCombo, 1);
	selectRow->addWidget(calcBtn);
	mainLayout->addLayout(selectRow);

	auto *table = new QTableWidget(dlg);
	table->setColumnCount(6);
	table->setHorizontalHeaderLabels({
		QStringLiteral("策略"),
		QStringLiteral("路径节点数"),
		QStringLiteral("总时间(s)"),
		QStringLiteral("总距离(m)"),
		QStringLiteral("拥堵指数"),
		QStringLiteral("换区次数")
	});
	table->horizontalHeader()->setStretchLastSection(true);
	table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	table->verticalHeader()->setVisible(false);
	table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	table->setSelectionBehavior(QAbstractItemView::SelectRows);
	table->setAlternatingRowColors(true);
	table->setRowCount(5);
	mainLayout->addWidget(table, 1);

	auto *bottomRow = new QHBoxLayout();
	bottomRow->setSpacing(12);
	auto *highlightLabel = new QLabel(QStringLiteral("选择高亮策略:"), dlg);
	auto *highlightCombo = new QComboBox(dlg);
	highlightCombo->addItem(QStringLiteral("时间最优"));
	highlightCombo->addItem(QStringLiteral("距离最短"));
	highlightCombo->addItem(QStringLiteral("避开拥挤"));
	highlightCombo->addItem(QStringLiteral("减少换区"));
	highlightCombo->addItem(QStringLiteral("综合均衡"));
	auto *highlightBtn = new QPushButton(QStringLiteral("在拓扑图上高亮"), dlg);
	highlightBtn->setFixedHeight(32);
	auto *clearBtn = new QPushButton(QStringLiteral("清除高亮"), dlg);
	clearBtn->setFixedHeight(32);
	clearBtn->setStyleSheet(QStringLiteral(
		"QPushButton{background:#E8DDD0;color:#3D322C;border:1px solid #C4A882;border-radius:8px;padding:8px 16px;font-weight:600;}"
		"QPushButton:hover{background:#D4C5B2;}"));
	bottomRow->addWidget(highlightLabel);
	bottomRow->addWidget(highlightCombo, 1);
	bottomRow->addWidget(highlightBtn);
	bottomRow->addWidget(clearBtn);
	bottomRow->addStretch();
	mainLayout->addLayout(bottomRow);

	struct PathResult {
		std::vector<std::string> path;
		PathMetrics metrics;
	};
	auto *results = new std::vector<PathResult>(5);

	connect(calcBtn, &QPushButton::clicked, dlg, [=]() {
		QString fromId = fromCombo->currentData().toString();
		QString toId = toCombo->currentData().toString();
		if (fromId == toId) {
			QMessageBox::warning(dlg, QStringLiteral("提示"), QStringLiteral("起点和终点不能相同。"));
			return;
		}

		PathPlanner planner;
		planner.setUseAStar(true);
		planner.setCacheEnabled(false);

		const auto &simNodeOcc = simulation_.nodeOccupancy();
		const auto &simEdgeOcc = simulation_.edgeOccupancy();

		const PathObjective objectives[] = {
			PathObjective::MinTime,
			PathObjective::MinDistance,
			PathObjective::MinCongestion,
			PathObjective::MinZoneSwitches,
			PathObjective::WeightedSum
		};
		const QString names[] = {
			QStringLiteral("时间最优"),
			QStringLiteral("距离最短"),
			QStringLiteral("避开拥挤"),
			QStringLiteral("减少换区"),
			QStringLiteral("综合均衡")
		};
		const QColor rowColors[] = {
			QColor("#4A9C8C"),
			QColor("#22558B"),
			QColor("#D4953A"),
			QColor("#8B5E7A"),
			QColor("#C43D3D")
		};

		std::vector<std::pair<std::vector<std::string>, QColor>> comparedPaths;

		for (int i = 0; i < 5; ++i) {
			auto path = planner.findPath(
				simulation_.graph(),
				fromId.toStdString(),
				toId.toStdString(),
				objectives[i],
					{},
					simEdgeOcc,
					simNodeOcc);

			PathMetrics metrics;
			if (!path.empty()) {
				metrics = planner.computePathMetrics(simulation_.graph(), path, simEdgeOcc, simNodeOcc);
				comparedPaths.push_back({path, rowColors[i]});
			}
			(*results)[i] = {path, metrics};

			auto *nameItem = new QTableWidgetItem(names[i]);
			nameItem->setForeground(rowColors[i]);
			nameItem->setFont(QFont(QString(), -1, QFont::Bold));
			table->setItem(i, 0, nameItem);

			if (path.empty()) {
				for (int col = 1; col < 6; ++col) {
					table->setItem(i, col, new QTableWidgetItem(QStringLiteral("--")));
				}
			} else {
				table->setItem(i, 1, new QTableWidgetItem(QString::number(static_cast<int>(path.size()))));
				table->setItem(i, 2, new QTableWidgetItem(QStringLiteral("%1").arg(metrics.totalTime, 0, 'f', 1)));
				table->setItem(i, 3, new QTableWidgetItem(QStringLiteral("%1").arg(metrics.totalDistance, 0, 'f', 1)));
				table->setItem(i, 4, new QTableWidgetItem(QStringLiteral("%1").arg(metrics.avgCongestion, 0, 'f', 3)));
				table->setItem(i, 5, new QTableWidgetItem(QString::number(metrics.zoneSwitches)));
			}
		}

		dashboard_->clearComparedPaths();
		dashboard_->setComparedPaths(comparedPaths);
	});

	connect(highlightBtn, &QPushButton::clicked, dlg, [=]() {
		int idx = highlightCombo->currentIndex();
		if (idx < 0 || idx >= 5) return;
		const auto &r = (*results)[idx];
		if (r.path.empty()) {
			QMessageBox::warning(dlg, QStringLiteral("提示"), QStringLiteral("该策略未找到有效路径，请先点击\"计算路径\"。"));
			return;
		}
		dashboard_->setHighlightedPath(r.path);
	});

	connect(clearBtn, &QPushButton::clicked, dlg, [=]() {
		dashboard_->setHighlightedPath({});
		dashboard_->clearComparedPaths();
	});

	connect(dlg, &QDialog::finished, this, [=]() {
		delete results;
	});

	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->show();
}

void MainWindow::open3DView()
{
	if (view3dDialog_) {
		view3dDialog_->raise();
		view3dDialog_->activateWindow();
		return;
	}

	view3dDialog_ = new QDialog(this);
	view3dDialog_->setAttribute(Qt::WA_DeleteOnClose);
	view3dDialog_->setWindowTitle(QStringLiteral("3D 车站视图 - %1").arg(QString::fromStdString(simulation_.graph().stationName())));
	view3dDialog_->resize(900, 650);
	view3dDialog_->setStyleSheet(QStringLiteral("QDialog{background:#2D2D30;}"));

	auto *layout = new QVBoxLayout(view3dDialog_);
	layout->setContentsMargins(0, 0, 0, 0);

	view3dWidget_ = new Station3DView(view3dDialog_);
	view3dWidget_->setGraph(simulation_.graph());
	layout->addWidget(view3dWidget_);

	auto *hint = new QLabel(QStringLiteral("左键旋转 | 右键平移 | 滚轮缩放"), view3dDialog_);
	hint->setStyleSheet(QStringLiteral("color:#AAAAAA;background:#3C3C40;padding:6px;font-size:12px;"));
	hint->setAlignment(Qt::AlignCenter);
	layout->addWidget(hint);

	connect(view3dDialog_, &QDialog::destroyed, this, [this]() {
		view3dDialog_ = nullptr;
		view3dWidget_ = nullptr;
	});

	view3dDialog_->show();
}
// end of file
