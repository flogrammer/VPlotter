#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QImage>
#include <QTimer>

#include "graphicseffects.h"
#include "convertform.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->b_openImage, SIGNAL (clicked()), this, SLOT (onClickOpenFile()));
    connect(ui->b_connect, SIGNAL (clicked()), this, SLOT (onClickConnect()));
    connect(ui->le_command, SIGNAL (returnPressed()), this, SLOT (onSubmitCmd()));

    connect(ui->sb_pos_x, SIGNAL (valueChanged(double)), this, SLOT (onChangeImgBounds()));
    connect(ui->sb_pos_y, SIGNAL (valueChanged(double)), this, SLOT (onChangeImgBounds()));
    connect(ui->sb_scale, SIGNAL (valueChanged(double)), this, SLOT (onChangeImgBounds()));

    connect(ui->b_pen_updown, SIGNAL (clicked()), this, SLOT (onClickPenUpDown()));
    connect(ui->b_move_down, SIGNAL (clicked()), this, SLOT (onClickMoveDown()));
    connect(ui->b_move_left, SIGNAL (clicked()), this, SLOT (onClickMoveLeft()));
    connect(ui->b_move_right, SIGNAL (clicked()), this, SLOT (onClickMoveRight()));
    connect(ui->b_move_up, SIGNAL (clicked()), this, SLOT (onClickMoveUp()));
    connect(ui->b_calib, SIGNAL (clicked()), this, SLOT (onClickCalibrate()));
    connect(ui->b_set_speed, SIGNAL (clicked()), this, SLOT (onClickSetSpeed()));
    connect(ui->b_execute,SIGNAL(clicked()),this,SLOT(onClickExecuteCmdFile()));
    connect(&serialPort, SIGNAL(readyRead()), this, SLOT(onTimerReadSerial()));
    connect(ui->b_simulate,SIGNAL(clicked()),this,SLOT(onClickSimulateCmdFile()));
    connect(ui->cb_nondrawing,SIGNAL(clicked()),this,SLOT(onChangeRenderOptions()));
    connect(ui->cb_penUD,SIGNAL(clicked()),this,SLOT(onChangeRenderOptions()));
    connect(ui->vp_plotterRenderer,SIGNAL(onSimulationFinished()),this,SLOT(onSimulationFinished()));
    connect(ui->b_convert,SIGNAL(clicked()),this,SLOT(onClickConvert()));
    connect(ui->b_load_cmdFile,SIGNAL(clicked()),this,SLOT(onClickOpenCmdFile()));

    timer = new QTimer();
    connect(timer,SIGNAL(timeout()),this,SLOT(onPollPosition()));
    //timer->start(5000);
    cmdListExec = new CommandListExecutor(&serialPort);

    connect(cmdListExec,SIGNAL(onExecutionAborted()),this,SLOT(onCmdExecFinished()));
    connect(cmdListExec,SIGNAL(onExecutionFinished()),this,SLOT(onCmdExecFinished()));
    connect(cmdListExec,SIGNAL(onSendCommand(QString)),this,SLOT(sendCmd(QString)));
    connect(this,SIGNAL(onStopCmdExec()),cmdListExec,SLOT(stop()));
    connect(this,SIGNAL(onSerialAnswerRecieved(QString)),cmdListExec,SLOT(onRecieveAnswer(QString)));
    connect(this,SIGNAL(onExecCmdList(QStringList)),cmdListExec,SLOT(executeCmdList(QStringList)));

    ui->vp_plotterRenderer->setPlotterSize(600,700);
    ui->sb_pos_x->setMaximum(600);
    ui->sb_pos_y->setMaximum(700);

    convertForm = new ConvertForm(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}
void MainWindow::printStatus(QString msg, bool error)
{
    QString html("<font color=\"%2\">%1</font><br>");

    QTextEdit textEdit;
    textEdit.setPlainText(msg);
    QString ret = textEdit.toHtml();
    html = html.arg(ret);

    if(error)
        html = html.arg("red");
    else
        html = html.arg("green");

    ui->te_output->moveCursor (QTextCursor::End);
    ui->te_output->insertHtml(html);
    ui->te_output->moveCursor (QTextCursor::End);
}
void MainWindow::setCommandList(QStringList cmds, bool autoSimulate)
{
    ui->te_comand_script->setPlainText(cmds.join("\n"));
    ui->l_editor_lines_of_code->setText(QString("%1").arg(cmds.length()));
    if(autoSimulate)
        onClickSimulateCmdFile();
}

void MainWindow::setPreprocessedImage(QImage img)
{
    ui->vp_plotterRenderer->setPreprocessedImage(img);
}

void MainWindow::onClickOpenFile()
{
    QString file = QFileDialog::getOpenFileName(this,
       "Open Image", "~", "Image Files (*.png *.jpg *.bmp)");

    if(file.isNull())
        return;

    if(!currentImage.load(file))
    {
        printStatus(QString("Can't open image: %1").arg(file),true);
        ui->gb_imageConvert->setEnabled(false);
        ui->vp_plotterRenderer->setRawImage(QImage());
        ui->vp_plotterRenderer->setPreprocessedImage(QImage());
        ui->vp_plotterRenderer->setImageBounds(0,0,0);

        return;
    }
    // Only use grayscale images
    currentImage = currentImage.convertToFormat(QImage::Format_Grayscale8);
    //currentImage = GraphicsEffects::applyBlur(currentImage);
    //currentImage = GraphicsEffects::applySobel(currentImage);
    //currentImage = GraphicsEffects::applyBinarize(currentImage,50,255,0);

    ui->vp_plotterRenderer->setRawImage(currentImage);
    ui->vp_plotterRenderer->setPreprocessedImage(QImage());
    float scale = std::min(ui->vp_plotterRenderer->getPlotterSize().x()/currentImage.width(),
                      ui->vp_plotterRenderer->getPlotterSize().y()/currentImage.height());
    ui->vp_plotterRenderer->setImageBounds(scale,0,0);
    convertForm->setImageInfo(currentImage,
                            QVector2D(0,0),scale);
    ui->vp_plotterRenderer->simulateCommands(QStringList());

    ui->sb_scale->setValue(scale);
    ui->sb_pos_x->setValue(0);
    ui->sb_pos_y->setValue(0);

    ui->gb_imageConvert->setEnabled(true);
}

void MainWindow::onClickConnect()
{
    if(serialPort.isOpen()){
        serialPort.close();
        ui->le_portName->setEnabled(true);
        ui->b_connect->setText("Connect");
        ui->le_command->setEnabled(false);
        ui->gb_calibration->setEnabled(false);
        ui->gb_manual->setEnabled(false);
        ui->b_execute->setEnabled(false);
        timer->stop();
        cmdListExec->stop();
        return;
    }
    QString port = ui->le_portName->text();
    serialPort.setPortName(port);

    if (!serialPort.open(QIODevice::ReadWrite)) {
        QString msg = QString("Can't open %1, error code %2!").arg(port).arg(serialPort.error());
        printStatus(msg,true);
    }else{
        ui->le_portName->setEnabled(false);
        ui->b_connect->setText("Disconnect");
        ui->le_command->setEnabled(true);
        ui->gb_calibration->setEnabled(true);
        ui->gb_manual->setEnabled(true);
        ui->b_execute->setEnabled(true);
        ui->te_output->clear();

        QString msg = QString("Connection to %1 successful!").arg(port);
        printStatus(msg);
    }
}
void MainWindow::onSubmitCmd()
{
    QString msg = ui->le_command->text();
    msg.append("\n");
    sendCmd(msg);

    ui->le_command->setText("");
}

void MainWindow::onChangeImgBounds()
{
    ui->vp_plotterRenderer->setImageBounds(ui->sb_scale->value(),ui->sb_pos_x->value(),ui->sb_pos_y->value());
    convertForm->setImageInfo(currentImage,
                              QVector2D(ui->sb_pos_x->value(),ui->sb_pos_y->value()),
                              ui->sb_scale->value());
}
void MainWindow::onTimerReadSerial()
{
    if(!serialPort.isOpen())
        return;

    while(serialPort.canReadLine()){
        QByteArray data = serialPort.readLine();
        if(data.length() > 0){
            ui->te_output->moveCursor (QTextCursor::End);

            QTextEdit textEdit;
            textEdit.setPlainText(QString(data));
            QString ret = textEdit.toHtml();

            ui->te_output->insertHtml(QString("<font color=\"black\">%1</font>").arg(ret));
            ui->te_output->moveCursor (QTextCursor::End);

            // Send data to cmd executor
            emit onSerialAnswerRecieved(QString(data));
        }
    }
}

void MainWindow::onClickLeftUp(){}
void MainWindow::onClickLeftDown(){}
void MainWindow::onClickRightUp(){}
void MainWindow::onClickRightDown(){}
void MainWindow::onClickMoveUp(){
    sendCmd("G91\n");
    sendCmd(QString("G0 Y-%1\n").arg(ui->sb_move_dist->value()));
    sendCmd("G90\n");
}
void MainWindow::onClickMoveLeft(){
    sendCmd("G91\n");
    sendCmd(QString("G0 X-%1\n").arg(ui->sb_move_dist->value()));
    sendCmd("G90\n");
}
void MainWindow::onClickMoveRight(){
    sendCmd("G91\n");
    sendCmd(QString("G0 X%1\n").arg(ui->sb_move_dist->value()));
    sendCmd("G90\n");
}
void MainWindow::onClickMoveDown(){
    sendCmd("G91\n");
    sendCmd(QString("G0 Y%1\n").arg(ui->sb_move_dist->value()));
    sendCmd("G90\n");
}
void MainWindow::onClickPenUpDown(){
    if(ui->b_pen_updown->text().compare("Pen Down")==0){
        sendCmd("M3\n");
        ui->b_pen_updown->setText("Pen Up");
    }else{
        sendCmd("M4\n");
        ui->b_pen_updown->setText("Pen Down");
    }
}
void MainWindow::onClickSetSpeed(){
    float speed = ui->sb_speed->value();
    sendCmd(QString("G0 F%1\n   ").arg(speed));
}
void MainWindow::onCmdExecFinished(){
    ui->b_execute->setText("Execute");
}

void MainWindow::sendCmd(QString msg){

    ui->te_output->moveCursor (QTextCursor::End);
    QTextEdit textEdit;
    textEdit.setPlainText(msg);
    QString ret = textEdit.toHtml();
    ui->te_output->insertHtml(QString("<font color=\"blue\">%1</font>").arg(ret));
    ui->te_output->moveCursor (QTextCursor::End);

    int sz = serialPort.write(msg.toLocal8Bit());

    if(sz<msg.length())
    {
        printStatus(QString("Failed to write all data (only %1 of %2 bytes)").arg(sz).arg(msg.length()),true);
    }
}
void MainWindow::onClickCalibrate()
{
    float b = ui->sb_calib_base->value();
    float l = ui->sb_calib_left->value();
    float r = ui->sb_calib_right->value();
    sendCmd(QString("M5 B%1 L%2 R%3\n").arg(b).arg(l).arg(r));
}
void MainWindow::onPollPosition(){
    serialPort.write("M8\n");
    QByteArray data = serialPort.readLine();
    QString strData(data);
    QStringList list = strData.split(" ");
    if(list.size() == 2)
        ui->l_pos->setText(list.at(0) + "x" + list.at(1));
    else
        ui->l_pos->setText("unknown...");
}

void MainWindow::onClickOpenCmdFile()
{
    QString name = QFileDialog::getOpenFileName(this,
       "Open Cmd File", "~", "Text Files (*.txt)");

    if(name.isNull())
        return;

    QFile file(name);
    if(!file.open(QIODevice::ReadOnly)) {
        printStatus(QString("Can't load cmd file %1").arg(name),true);
        return;
    }
    QString txt = file.readAll();
    ui->te_comand_script->setPlainText(txt);
}

void MainWindow::onClickExecuteCmdFile()
{
    if(ui->b_execute->text().compare("Execute") == 0){
        QStringList cmds = ui->te_comand_script->toPlainText().split("\n");
        emit onExecCmdList(cmds);
        ui->b_execute->setText("Stop execution");
    }else{
        emit onStopCmdExec();
        ui->b_execute->setText("Execute");
    }
}
void MainWindow::onClickSimulateCmdFile()
{
    if(ui->b_simulate->text().compare("Simulate") == 0){
        QStringList cmds = ui->te_comand_script->toPlainText().split("\n");
        ui->vp_plotterRenderer->simulateCommands(cmds);
        ui->b_simulate->setText("Stop Simulation");
    }else{
        ui->vp_plotterRenderer->abortSimulation();
    }
}
void MainWindow::onChangeRenderOptions()
{
    ui->vp_plotterRenderer->setRenderOptions(ui->cb_nondrawing->isChecked(),
                                             ui->cb_penUD->isChecked());
}
void MainWindow::onSimulationFinished()
{
    ui->b_simulate->setText("Simulate");
}

void MainWindow::onClickConvert()
{
    convertForm->show();
}
