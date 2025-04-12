#ifndef FORM_H
#define FORM_H

#include <QSettings>
#include <QWidget>

namespace Ui {
class Form;
}

class Form : public QWidget {
    Q_OBJECT

public:
    explicit Form(QWidget* parent = nullptr);
    ~Form();

    void closeEvent(QCloseEvent* e) override;

private:
    Ui::Form* ui;
};

#endif // FORM_H
