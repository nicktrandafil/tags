#include "form.h"

#include "ui_form.h"

constexpr auto line_tags = "line edit tags";
constexpr auto box_tags = "box edit tags";

Form::Form(QWidget* parent) : QWidget(parent), ui(new Ui::Form) {
    ui->setupUi(this);

    QSettings settings;

    {
        auto const tags = settings.value(line_tags).value<QVector<QString>>();
        ui->widget->tags(std::vector<QString>{tags.begin(), tags.end()});
    }

    {
        auto const tags = settings.value(box_tags).value<QVector<QString>>();
        ui->widget_2->tags(std::vector<QString>{tags.begin(), tags.end()});
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
}
