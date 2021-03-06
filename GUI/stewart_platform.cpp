#include "stewart_platform.h"

StewartPlatform::StewartPlatform(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::StewartPlatform),
    m_settings(new SerialSettingsDialog),
    m_serial(new QSerialPort(this)),
    leap(new LeapEventListener())
{
    ui->setupUi(this);

    actuator_positions = {0, 0, 0, 0, 0, 0};

    /** Connect manual control signals **/
    manual_fields = {ui->field_1, ui->field_2, ui->field_3, ui->field_4, ui->field_5, ui->field_6};
    manual_sliders = {ui->slider_1, ui->slider_2, ui->slider_3, ui->slider_4, ui->slider_5, ui->slider_6};

    // Setup signals for each field/slider pair
    for (int i = 0; i < NUM_ACTUATORS; ++i)
    {
        field = manual_fields[i];
        slider = manual_sliders[i];

        // Set max and min for each actuator
        field->setMinimum(MIN_ACTUATOR_VALUE);
        field->setMaximum(MAX_ACTUATOR_VALUE);
        slider->setMinimum(MIN_ACTUATOR_VALUE);
        slider->setMaximum(MAX_ACTUATOR_VALUE);

        // Slider updates spinbox and spinbox updates sliders
        connect(field, QOverload<int>::of(&QSpinBox::valueChanged), slider, &QSlider::setValue);
        connect(slider, &QSlider::valueChanged, field, &QSpinBox::setValue);

        // Slider updates actuator_positions (lambda selects appropriate index)
        connect(slider, &QSlider::valueChanged, this, [=](int j){this->actuator_positions[i] = j;});
    }

    // Initialize status labels
    ui->label_serial_val->setText(tr("Disconnected"));
    ui->label_leap_val->setText(tr("Disconnected"));
    connect(leap, &LeapEventListener::LeapConnected, this, &StewartPlatform::onLeapConnected);

    // Send button deinitialized
    ui->button_send->setEnabled(false);

    /** Serial-related signals **/
    // Serial logging
    connect(m_serial, &QSerialPort::readyRead, this, &StewartPlatform::readSerialData);

    // Serial Settings dialog
    connect(ui->actionSelect_COM_port, &QAction::triggered, m_settings, &SerialSettingsDialog::ShowAndUpdatePortInfo);

    // Connect to serial port
    connect(m_settings, &SerialSettingsDialog::SettingsUpdated, this, &StewartPlatform::openSerialPort);

    // Disconnect from serial port if something happens
    // todo: make dedicated error handling function, for now disconnecting the port is good enough
    connect(m_serial, &QSerialPort::errorOccurred, this, &StewartPlatform::closeSerialPort);

    // Send actuator_positions values over serial
    connect(ui->button_send, &QPushButton::clicked, this, [=](){this->sendActuatorPositions(this->actuator_positions);});

    /** Leap Motion signals **/
    // Leap Motion checkbox disables actuator box, update leap_enabled variable
    connect(ui->enable_leap_motion, &QCheckBox::toggled, ui->actuatorBox, [=](bool c){enableLeapMotion(c);});
    connect(leap, &LeapEventListener::LeapFrameUpdate, this, &StewartPlatform::sendActuatorPositions);
}

StewartPlatform::~StewartPlatform()
{
    delete ui;
    delete m_settings;

    closeSerialPort();
    delete m_serial;

    delete leap;
}

void StewartPlatform::on_actionExit_triggered()
{
    QApplication::quit();
}

void StewartPlatform::log(const QString& entry)
{
    ui->log->appendPlainText("(" + QTime::currentTime().toString() + ") " + entry);
}

void StewartPlatform::sendActuatorPositions(QVector<int> actuator_pos)
{
    QString s = "";
    for(int i = 0; i < NUM_ACTUATORS; ++i)
    {
        // Convert each actuator int to string, add space if not last element, else new line
        s += QString::number(actuator_pos[i]) + ((i < (NUM_ACTUATORS - 1)) ? " " : "\n");
    }
    writeSerialData(qPrintable(s));
}

void StewartPlatform::readSerialData()
{
    Q_ASSERT(m_serial->isOpen());
    log(m_serial->readAll());
//    ui->log->appendPlainText(m_serial->readAll());
}

void StewartPlatform::writeSerialData(const char* data)
{
    Q_ASSERT(m_serial->isOpen());
    m_serial->write(data);
}

void StewartPlatform::openSerialPort()
{
    // Close the existing serial handle
    closeSerialPort();

    // Set configuration received from the serial dialog box
    const SerialSettingsDialog::Settings p = m_settings->settings();
    m_serial->setPortName(p.name);
    m_serial->setBaudRate(p.baudRate);
    m_serial->setDataBits(p.dataBits);
    m_serial->setParity(p.parity);
    m_serial->setStopBits(p.stopBits);
    m_serial->setFlowControl(p.flowControl);

    // If able to connect to the port, update the status and enable the send button
    // Else, produce an error
    if (m_serial->open(QIODevice::ReadWrite))
    {
        ui->label_serial_val->setText(tr("%1").arg(p.name));
        ui->button_send->setEnabled(true);

        log(
            tr("<COM>  Connected to %1 : %2, %3, %4, %5, %6")
                .arg(p.name)
                .arg(p.stringBaudRate)
                .arg(p.stringDataBits)
                .arg(p.stringParity)
                .arg(p.stringStopBits)
                .arg(p.stringFlowControl)
        );
    }
    else
    {
        log(tr("<COM>  Connection error: %1").arg(m_serial->errorString()));
    }
}

void StewartPlatform::closeSerialPort()
{
    if (m_serial->isOpen())
    {
        m_serial->close();
        log(tr("<COM>  Disconnected from %1").arg(m_serial->portName()));

        // Update status, disable send button
        ui->label_serial_val->setText(tr("Disconnected"));
        ui->button_send->setEnabled(false);
    }
}

void StewartPlatform::enableLeapMotion(bool c)
{
    ui->actuatorBox->setEnabled(!c);
    leap->is_leap_enabled = c;
}

void StewartPlatform::onLeapConnected(bool connected)
{
    if (connected)
    {
        ui->label_leap_val->setText(tr("Connected"));
        log(tr("<LEAP> Connected"));

    }
    else
    {
        ui->label_leap_val->setText(tr("Disconnected"));
        log(tr("<LEAP> Disconnected"));
    }
}
