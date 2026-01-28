#include "form.h"

#include "ui_form.h"

#include <QFontMetrics>
#include <QLabel>
#include <QPainter>
#include <QPoint>

constexpr auto line_tags = "line edit tags";
constexpr auto box_tags = "box edit tags";
constexpr auto line_tags2 = "line edit tags 2";
constexpr auto box_tags2 = "box edit tags 2";

using namespace everload_tags;
using namespace std;

struct MyWidget : QWidget {
    std::vector<Tag> tags;
    StyleConfig style{};

    MyWidget(QWidget* parent) : QWidget{parent} {}

    void paintEvent(QPaintEvent* e) override {
        QWidget::paintEvent(e);
        QPainter p(this);

        QPoint lt{};
        style.calcRects(lt, tags, fontMetrics(), rect(), false);

        style.drawTags(p, tags, fontMetrics(), {}, false);
    }

    QSize minimumSizeHint() const override {
        return QSize{40, style.pillHeight(this->fontMetrics().height())};
    }
};

Form::Form(QWidget* parent) : QWidget(parent), ui(new Ui::Form) {
    ui->setupUi(this);

    StyleConfig style{
        .pill_thickness = {7, 7, 8, 7},
        .pills_h_spacing = 7,
        .tag_cross_size = 8,
        .tag_cross_spacing = 3,
        .color = {255, 7, 100, 100},
        .rounding_x_radius = 5,
        .rounding_y_radius = 10,
    };

    BehaviorConfig behavior{
        .unique = false,
        .restore_cursor_position_on_focus_click = true,
    };

    QSettings settings;

    {
        auto const tags = settings.value(line_tags).value<QVector<QString>>();
        ui->le->tags(vector<QString>{tags.begin(), tags.end()});
    }

    {
        auto const tags = settings.value(line_tags).value<QVector<QString>>();
        ui->ro->config(Config{.behavior = BehaviorConfig{.read_only = true}});
        ui->ro->tags(vector<QString>{tags.begin(), tags.end()});
    }

    {
        auto const tags = settings.value(line_tags2).value<QVector<QString>>();
        ui->tl_custom_style->tags(vector<QString>{tags.begin(), tags.end()});
        ui->tl_custom_style->config(Config{.style = style, .behavior = behavior});
    }

    {
        auto const tags = settings.value(box_tags).value<QVector<QString>>();
        ui->te->tags(vector<QString>{tags.begin(), tags.end()});
        ui->te->config(Config{.behavior = behavior});
    }

    {
        auto const tags = settings.value(box_tags2).value<QVector<QString>>();
        ui->te_custom_style->tags(vector<QString>{tags.begin(), tags.end()});
        ui->te_custom_style->config(Config{.style = style});

        auto widget_5 = new MyWidget(this);
        ranges::transform(tags, back_inserter(widget_5->tags),
                          [](auto const& str) { return Tag{.text = str, .rect = {}}; });
        ui->verticalLayout->addWidget(new QLabel{"MyWidget (uses calcRects() and drawTags()):"});
        ui->verticalLayout->addWidget(widget_5);
    }

    {
        auto const tags = settings.value(box_tags2).value<QVector<QString>>();
        ui->te_ro->tags(vector<QString>{tags.begin(), tags.end()});
        ui->te_ro->config(Config{.behavior = BehaviorConfig{.read_only = true}});
    }
}

Form::~Form() {
    delete ui;
}

void Form::closeEvent(QCloseEvent* e) {
    QWidget::closeEvent(e);
    QSettings settings;

    {
        auto const tags = ui->le->tags();
        settings.setValue(line_tags, QVector<QString>(tags.begin(), tags.end()));
    }

    {
        auto const tags = ui->te->tags();
        settings.setValue(box_tags, QVector<QString>(tags.begin(), tags.end()));
    }

    {
        auto const tags = ui->tl_custom_style->tags();
        settings.setValue(line_tags2, QVector<QString>(tags.begin(), tags.end()));
    }

    {
        auto const tags = ui->te_custom_style->tags();
        settings.setValue(box_tags2, QVector<QString>(tags.begin(), tags.end()));
    }
}
