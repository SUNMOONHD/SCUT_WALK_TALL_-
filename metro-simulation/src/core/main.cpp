#include "main.h"

#ifdef METRO_SIM_ENABLE_GUI
#include "mainwindow.h"
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QStyleFactory>
#include <QToolTip>
#include <QVBoxLayout>
#endif

#include "report_writer.h"
#include "result_export.h"
#include "simulation.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace {

std::string resolvePath(int argc, char *argv[], int index, const std::string &fallback)
{
	if (argc > index && argv[index] != nullptr && std::string(argv[index]).empty() == false) {
		return argv[index];
	}

	return fallback;
}

} // namespace

int runApplication(int argc, char *argv[])
{
	std::filesystem::path exeDir;
	try {
		exeDir = std::filesystem::canonical(std::filesystem::path(argv[0])).parent_path();
	} catch (...) {
		try {
			exeDir = std::filesystem::current_path();
		} catch (...) {
		}
	}

	auto relativePath = [&exeDir](const std::string &relative) -> std::string {
		if (exeDir.empty()) {
			return relative;
		}
		return (exeDir / relative).string();
	};

	const std::string stationPath = resolvePath(argc, argv, 1, relativePath("data/stations/sample_station.json"));
	const std::string paramsPath = resolvePath(argc, argv, 2, relativePath("data/params/default_params.json"));

	Simulation simulation;
	std::string error;
	if (!simulation.loadScenario(stationPath, paramsPath, &error)) {
		std::cerr << "[metro_sim] Failed to load scenario\n"
			      << "  station: " << stationPath << "\n"
			      << "  params : " << paramsPath << "\n"
			      << "  reason : " << error << "\n";
		return EXIT_FAILURE;
	}

	std::cout << "[metro_sim] Scenario loaded successfully\n"
		  << "  station: " << simulation.graph().stationName() << "\n"
		  << "  floors : " << simulation.graph().floors().size() << "\n"
		  << "  nodes  : " << simulation.graph().nodeCount() << "\n"
		  << "  edges  : " << simulation.graph().edgeCount() << "\n"
		  << "  dt(s)  : " << simulation.config().timeStep << "\n"
		  << "  peak_lambda : " << simulation.config().peakLambda << "\n";

	for (int i = 0; i < 60; ++i) {
		simulation.step();
	}
	std::cout << "[metro_sim] Warmup run complete, t=" << simulation.currentTime() << "s\n";

	const results::ExportPaths exportPaths{
		relativePath("data/results/step3_summary.json"),
		relativePath("data/results/step3_events.csv")
	};
	std::string exportError;
	if (!results::exportStep3Results(simulation, exportPaths, &exportError)) {
		std::cerr << "[metro_sim] Result export failed: " << exportError << "\n";
		return EXIT_FAILURE;
	}
	std::cout << "[metro_sim] Result export complete\n"
		  << "  summary: " << exportPaths.summaryJsonPath << "\n"
		  << "  events : " << exportPaths.eventsCsvPath << "\n";

	const std::string reportPath = relativePath("data/results/step3_report.html");
	std::string reportError;
	if (!reports::writeStep3HtmlReport(simulation, reportPath, &reportError)) {
		std::cerr << "[metro_sim] Report generation failed: " << reportError << "\n";
		return EXIT_FAILURE;
	}
	std::cout << "[metro_sim] Report generated\n"
		  << "  report : " << reportPath << "\n";
	return EXIT_SUCCESS;
}

#ifdef METRO_SIM_ENABLE_GUI

namespace {

void applyApplicationStyle(QApplication &application)
{
	application.setStyle(QStyleFactory::create("Fusion"));
	QPalette palette;
	palette.setColor(QPalette::Window, QColor(245, 240, 232));
	palette.setColor(QPalette::WindowText, QColor(61, 50, 44));
	palette.setColor(QPalette::Base, QColor(254, 250, 243));
	palette.setColor(QPalette::AlternateBase, QColor(237, 228, 216));
	palette.setColor(QPalette::ToolTipBase, QColor(245, 237, 224));
palette.setColor(QPalette::ToolTipText, QColor(44, 36, 24));
	palette.setColor(QPalette::Text, QColor(61, 50, 44));
	palette.setColor(QPalette::Button, QColor(237, 228, 216));
	palette.setColor(QPalette::ButtonText, QColor(61, 50, 44));
	palette.setColor(QPalette::Highlight, QColor(196, 61, 61));
	palette.setColor(QPalette::HighlightedText, QColor(254, 250, 243));
	application.setPalette(palette);

	QPalette tipPalette;
	tipPalette.setColor(QPalette::ToolTipBase, QColor(245, 237, 224));
	tipPalette.setColor(QPalette::ToolTipText, QColor(44, 36, 24));
	QToolTip::setPalette(tipPalette);

	application.setStyleSheet(R"(
		QMainWindow { background: #F5F0E8; }
		QToolTip { background: #F5EDE0; color: #2C2418; border: 1px solid #C4A882; border-radius: 4px; padding: 4px; }
		QToolBar { background: #EDE4D8; border: none; border-bottom: 1px solid #D4C5B2; spacing: 8px; padding: 6px; }
		QToolButton {
			background: #FEFAF3; color: #3D322C; border: 1px solid #D4C5B2; border-radius: 8px;
			padding: 6px 12px; font-weight: 600;
		}
		QToolButton:hover { background: #F5EDE0; border-color: #C43D3D; }
		QToolButton:checked { background: #C43D3D; color: #FEFAF3; border-color: #C43D3D; }
		QComboBox {
			background: #FEFAF3; color: #3D322C; border: 1px solid #D4C5B2; border-radius: 6px;
			padding: 4px 10px;
		}
		QComboBox:hover { border-color: #C43D3D; }
		QComboBox QAbstractItemView {
			background: #FEFAF3; color: #3D322C; selection-background-color: #C43D3D;
			selection-color: #FEFAF3; border: 1px solid #D4C5B2;
		}
		QFrame#CardFrame { background: #FEFAF3; border: 1px solid #E0D5C5; border-radius: 14px; }
		QLabel#Headline { color: #3D322C; font-size: 20px; font-weight: 700; }
		QLabel#Subtle { color: #8B7D6B; }
		QPlainTextEdit { background: #FDF8F0; color: #3D322C; border: 1px solid #E0D5C5; border-radius: 12px; }
		QSplitter::handle { background: #E0D5C5; }
		QStatusBar { background: #EDE4D8; color: #8B7D6B; border-top: 1px solid #D4C5B2; }
		QScrollBar:vertical {
			background: #F5F0E8; width: 10px; border-radius: 5px;
		}
		QScrollBar::handle:vertical {
			background: #D4C5B2; border-radius: 5px; min-height: 30px;
		}
		QScrollBar::handle:vertical:hover { background: #C43D3D; }
		QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
	)"
	);
}

} // namespace

int main(int argc, char *argv[])
{
	QApplication application(argc, argv);
	applyApplicationStyle(application);

	const QString appDir = QCoreApplication::applicationDirPath();
	const QStringList resourceDirs = {
		appDir + QStringLiteral("/resources/"),
		appDir + QStringLiteral("/../resources/"),
		appDir + QStringLiteral("/../../resources/"),
		QDir::currentPath() + QStringLiteral("/resources/"),
	};
	auto resolveResource = [&](const QString &relativePath) -> QString {
		for (const auto &dir : resourceDirs) {
			QString path = QDir(dir).absoluteFilePath(relativePath);
			if (QFile::exists(path))
				return path;
		}
		return relativePath;
	};

	QDialog splash;
	splash.setWindowTitle(QStringLiteral("地铁综合换乘站"));
	splash.setFixedSize(480, 380);
	splash.setStyleSheet(QStringLiteral(
		"QDialog { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
		"stop:0 #FEFAF3, stop:1 #F0E8D8); }"));

	auto *scutLogo = new QLabel(&splash);
	QPixmap scutPixmap(resolveResource(QStringLiteral("ui/scut_logo.png")));
	if (!scutPixmap.isNull()) {
		scutLogo->setPixmap(scutPixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
	}
	scutLogo->setFixedSize(64, 64);
	scutLogo->move(splash.width() - 76, 12);
	scutLogo->setStyleSheet(QStringLiteral("background:transparent;"));

	auto *splashLayout = new QVBoxLayout(&splash);
	splashLayout->setAlignment(Qt::AlignCenter);
	splashLayout->setSpacing(16);
	splashLayout->setContentsMargins(40, 20, 40, 20);

	auto *logoLabel = new QLabel(&splash);
	QPixmap logoPixmap(resolveResource(QStringLiteral("ui/splash_logo.png")));
	if (!logoPixmap.isNull()) {
		logoLabel->setPixmap(logoPixmap.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
	} else {
		logoLabel->setText(QStringLiteral("🚇"));
		logoLabel->setStyleSheet(QStringLiteral("font-size:64px;"));
	}
	logoLabel->setAlignment(Qt::AlignCenter);
	splashLayout->addWidget(logoLabel);

	auto *titleLabel = new QLabel(QStringLiteral("地铁综合换乘站"), &splash);
	titleLabel->setStyleSheet(QStringLiteral(
		"font-size:28px; font-weight:700; color:#3D322C;"));
	titleLabel->setAlignment(Qt::AlignCenter);
	splashLayout->addWidget(titleLabel);

	auto *subtitleLabel = new QLabel(QStringLiteral("Metro Station Passenger Flow Simulation"), &splash);
	subtitleLabel->setStyleSheet(QStringLiteral(
		"font-size:13px; color:#8B7D6B;"));
	subtitleLabel->setAlignment(Qt::AlignCenter);
	splashLayout->addWidget(subtitleLabel);

	splashLayout->addSpacing(12);

	auto *enterBtn = new QPushButton(QStringLiteral("进入仿真"), &splash);
	enterBtn->setFixedSize(180, 44);
	enterBtn->setStyleSheet(QStringLiteral(
		"QPushButton { background:#C43D3D; color:#FEFAF3; border:none; "
		"border-radius:22px; font-size:16px; font-weight:600; }"
		"QPushButton:hover { background:#A83232; }"));
	enterBtn->setCursor(Qt::PointingHandCursor);
	QObject::connect(enterBtn, &QPushButton::clicked, &splash, &QDialog::accept);
	splashLayout->addWidget(enterBtn, 0, Qt::AlignCenter);

	auto *versionLabel = new QLabel(QStringLiteral("v1.1 — 基于 Qt6 开发"), &splash);
	versionLabel->setStyleSheet(QStringLiteral("font-size:11px; color:#B8A898;"));
	versionLabel->setAlignment(Qt::AlignCenter);
	splashLayout->addWidget(versionLabel);

	if (splash.exec() != QDialog::Accepted) {
		return 0;
	}

	MainWindow *window = new MainWindow();
	window->setAttribute(Qt::WA_DeleteOnClose);
	window->show();
	return application.exec();
}

#else

int main(int argc, char *argv[])
{
	return runApplication(argc, argv);
}

#endif
