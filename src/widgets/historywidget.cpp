#include "historywidget.h"
#include "src/utils/history.h"
#include "src/widgets/notificationwidget.h"
#include "src/tools/imgs3/imgs3uploader.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QLabel>
#include <QScrollArea>
#include <QFileInfo>
#include <QDateTime>
#include <QPushButton>
#include <QIcon>
#include <QSettings>
#include <QApplication>
#include <QDesktopWidget>
#include <QDesktopServices>
#include <QClipboard>
#include <QUrl>
#include <QLayoutItem>


HistoryWidget::HistoryWidget(QWidget *parent) : QDialog(parent)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowTitle(tr("Latest Uploads"));
    setFixedSize(800, this->height());
    m_notification = new NotificationWidget();

    m_pVBox = new QVBoxLayout(this);
    m_pVBox->setAlignment(Qt::AlignTop);

    QScrollArea *scrollArea = new QScrollArea( this );
    scrollArea->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOn );
    scrollArea->setWidgetResizable( true );
    scrollArea->setGeometry( this->frameGeometry() );

    QWidget *widget = new QWidget();
    scrollArea->setWidget( widget );
    widget->setLayout( m_pVBox );

    loadHistory();
}

void HistoryWidget::loadHistory() {
    // read history files
    History history = History();
    QList<QString> historyFiles = history.history();

    if(historyFiles.isEmpty()) {
        setEmptyMessage();
    }
    else {
        // generate history list
        foreach(QString fileName, historyFiles) {
            addLine(history.path(), fileName);
        }
    }
}

void HistoryWidget::setEmptyMessage() {
    QPushButton *buttonEmpty = new QPushButton;
    buttonEmpty->setText(tr("Screenshots history is empty"));
    buttonEmpty->setMinimumSize(1, HISTORYPIXMAP_MAX_PREVIEW_HEIGHT);
    connect(buttonEmpty, &QPushButton::clicked, this, [=](){
        this->close();
    });
    m_pVBox->addWidget(buttonEmpty);
}

void HistoryWidget::addLine(const QString &path, const QString& fileName) {
    QHBoxLayout *phbl = new QHBoxLayout();
    QString fullFileName = path + fileName;

    History history;
    HISTORY_FILE_NAME unpackFileName = history.unpackFileName(fileName);

    QString url = m_s3Settings.url() + unpackFileName.file;

    // load pixmap
    QPixmap pixmap;
    pixmap.load( fullFileName, "png" );

    if (pixmap.height() / HISTORYPIXMAP_MAX_PREVIEW_HEIGHT >= pixmap.width() / HISTORYPIXMAP_MAX_PREVIEW_WIDTH) {
        pixmap = pixmap.scaledToHeight(HISTORYPIXMAP_MAX_PREVIEW_HEIGHT);
    } else {
        pixmap = pixmap.scaledToWidth(HISTORYPIXMAP_MAX_PREVIEW_WIDTH);
    }

    // get file info
    QFileInfo *pFileInfo = new QFileInfo(fullFileName);
    QString lastModified = pFileInfo->lastModified().toString(" yyyy-MM-dd\nhh:mm:ss");

    // screenshot preview
    QLabel *pScreenshot = new QLabel();
    pScreenshot->setStyleSheet("padding: 5px;");
    pScreenshot->setMinimumHeight(HISTORYPIXMAP_MAX_PREVIEW_HEIGHT);
    pScreenshot->setPixmap(pixmap);

    // screenshot datetime
    QLabel *pScreenshotText = new QLabel();
    pScreenshotText->setStyleSheet("padding: 5px;");
    pScreenshotText->setMinimumHeight(HISTORYPIXMAP_MAX_PREVIEW_HEIGHT);
    pScreenshotText->setAlignment(Qt::AlignCenter);
    pScreenshotText->setText(lastModified);

    // copy url
    QPushButton *buttonCopyUrl = new QPushButton;
    buttonCopyUrl->setText(tr("Copy URL"));
    buttonCopyUrl->setMinimumHeight(HISTORYPIXMAP_MAX_PREVIEW_HEIGHT);
    connect(buttonCopyUrl, &QPushButton::clicked, this, [=](){
        QApplication::clipboard()->setText(url);
        m_notification->showMessage(tr("URL copied to clipboard."));
        this->close();
    });

    // open in browser
    QPushButton *buttonOpen = new QPushButton;
    buttonOpen->setText(tr("Open in browser"));
    buttonOpen->setMinimumHeight(HISTORYPIXMAP_MAX_PREVIEW_HEIGHT);
    connect(buttonOpen, &QPushButton::clicked, this, [=](){
        QDesktopServices::openUrl(QUrl(url));
        this->close();
    });

    // delete
    QPushButton *buttonDelete = new QPushButton;
    buttonDelete->setIcon(QIcon(":/img/material/black/delete.svg"));
    buttonDelete->setMinimumHeight(HISTORYPIXMAP_MAX_PREVIEW_HEIGHT);
    connect(buttonDelete, &QPushButton::clicked, this, [=](){
        if (unpackFileName.token.length() > 0) {
            removeItem(phbl, unpackFileName.file, unpackFileName.token);
        }
        else {
            // for compatibility with previous versions and to be able to remove previous screenshots
            QFile file(fullFileName);
            if (file.exists()) {
                file.remove();
            }
            removeLocalItem(phbl);
        }
    });

    // layout
    phbl->addWidget(pScreenshot);
    phbl->addWidget(pScreenshotText);
    phbl->addWidget(buttonCopyUrl);
    phbl->addWidget(buttonOpen);
    phbl->addWidget(buttonDelete);

    phbl->setStretchFactor(pScreenshot, 6);
    phbl->setStretchFactor(pScreenshotText, 4);
    phbl->setStretchFactor(buttonCopyUrl, 4);
    phbl->setStretchFactor(buttonOpen, 4);
    phbl->setStretchFactor(buttonDelete, 1);

    // add to scroll
    m_pVBox->addLayout(phbl);
}

void HistoryWidget::removeItem(QLayout *pl, const QString& s3FileName, const QString& deleteToken) {
    ImgS3Uploader *uploader = new ImgS3Uploader();
    hide();
    uploader->show();
    uploader->deleteResource(s3FileName, deleteToken);
    connect(uploader, &QWidget::destroyed, this, [=](){
        if(uploader->success()) {
            removeLocalItem(pl);
        }
        show();
    });
}

void HistoryWidget::removeLocalItem(QLayout *pl) {
    // remove current row or refresh list
    while(pl->count() > 0) {
        QLayoutItem *item = pl->takeAt(0);
        delete item->widget();
        delete item;
    }
    m_pVBox->removeItem(pl);
    delete pl;

    // set "empty" message if no items left
    if(m_pVBox->count() == 0) {
        setEmptyMessage();
    }
}
