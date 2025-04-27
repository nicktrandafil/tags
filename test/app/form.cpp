#include "form.h"

#include "ui_form.h"

constexpr auto line_tags = "line edit tags";
constexpr auto box_tags = "box edit tags";
constexpr auto line_tags2 = "line edit tags 2";
constexpr auto box_tags2 = "box edit tags 2";

using namespace everload_tags;

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

    QSettings settings;

    {
        auto const tags = settings.value(line_tags).value<QVector<QString>>();
        ui->widget->tags(std::vector<QString>{tags.begin(), tags.end()});
    }

    {
        auto const tags = settings.value(box_tags).value<QVector<QString>>();
        ui->widget_2->tags(std::vector<QString>{tags.begin(), tags.end()});
    }

    {
        auto const tags = settings.value(line_tags2).value<QVector<QString>>();
        ui->widget_3->tags(std::vector<QString>{tags.begin(), tags.end()});
        ui->widget_3->config(Config{.style = style});
    }

    {
        auto const tags = settings.value(box_tags2).value<QVector<QString>>();
        ui->widget_4->tags(std::vector<QString>{tags.begin(), tags.end()});
        ui->widget_4->config(Config{.style = style});
    }
}

Form::~Form() {
    delete ui;
}

void Form::closeEvent(QCloseEvent* e) {
    QWidget::closeEvent(e);
    QSettings settings;

    {
        auto const tags = ui->widget->tags();
        settings.setValue(line_tags, QVector<QString>(tags.begin(), tags.end()));
    }

    {
        auto const tags = ui->widget_2->tags();
        settings.setValue(box_tags, QVector<QString>(tags.begin(), tags.end()));
    }

    {
        auto const tags = ui->widget_3->tags();
        settings.setValue(line_tags2, QVector<QString>(tags.begin(), tags.end()));
    }

    {
        auto const tags = ui->widget_4->tags();
        settings.setValue(box_tags2, QVector<QString>(tags.begin(), tags.end()));
    }
}
