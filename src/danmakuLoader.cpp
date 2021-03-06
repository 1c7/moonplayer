/* Copyright 2013-2020 Yikun Liu <cos.lyk@gmail.com>
 *
 * This program is free software: you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see http://www.gnu.org/licenses/.
 */

#include "danmakuLoader.h"
#include "accessManager.h"
#include <QApplication>
#include <QDesktopWidget>
#include <QFont>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <Danmaku2ASS/CommentParser.h>
#include <sstream>
#include "mpvObject.h"

DanmakuLoader DanmakuLoader::s_instance;


DanmakuLoader::DanmakuLoader(QObject *parent) :
    QObject(parent), m_reply(nullptr)
{
}



// Start
void DanmakuLoader::start(const QUrl& srcUrl, int width, int height)
{
    Q_ASSERT(QApplication::desktop() != nullptr);
    Q_ASSERT(NetworkAccessManager::instance() != nullptr);

    // Set video size
    if (height > QApplication::desktop()->height())
    {
        m_height = QApplication::desktop()->height();
        m_width = width * QApplication::desktop()->height() / height;
    }
    else if (width > QApplication::desktop()->width())
    {
        m_height = height * QApplication::desktop()->width() / width;
        m_width = QApplication::desktop()->width();
    }
    else
    {
        m_width = width;
        m_height = height;
    }
    
    //another task is running?
    if (m_reply != nullptr)
    {
        m_reply->disconnect();
        m_reply->abort();
        m_reply->deleteLater();
    }
    m_reply = NetworkAccessManager::instance()->get(QNetworkRequest(srcUrl));
    connect(m_reply, &QNetworkReply::finished, this, &DanmakuLoader::onXmlDownloaded);
}



void DanmakuLoader::onXmlDownloaded()
{
    Q_ASSERT(m_reply != nullptr);
    Q_ASSERT(MpvObject::instance() != nullptr);

    if (m_reply->error() == QNetworkReply::NoError)
    {
        // load settings
        QSettings settings;

        // Font
        QString fontName = settings.value(QStringLiteral("danmaku/font")).value<QFont>().family();
        if (fontName.isEmpty())
        {
#ifdef Q_OS_MAC
            fontName = QStringLiteral("PingFang SC");
#else
            fontName = QStringLiteral("sans-serif");
#endif
        }
        
        // Font size
        int fontSize = settings.value(QStringLiteral("danmaku/font_size")).toInt();
        if (fontSize == 0)
        {
            if (m_width > 960)
                fontSize = 36;
            else if (m_width > 640)
                fontSize = 32;
            else
                fontSize = 28;
        }

        // Duration of comment display
        int dm = settings.value(QStringLiteral("danmaku/dm")).toInt();
        if (dm == 0)
        {
            if (m_width > 960)
                dm = 10;
            else if (m_width > 640)
                dm = 8;
            else
                dm = 6;
        }

        // Duration of still danmaku
        int ds = settings.value(QStringLiteral("danmaku/ds")).toInt();

        // text opacity
        double alpha = settings.value(QStringLiteral("danmaku/alpha")).toDouble() / 100.0;

        // Create parser
        QByteArray source = m_reply->readAll();
        std::stringstream input(source.toStdString());

        Danmaku2ASS::CommentParser parser(input);
        parser.setResolution(m_width, m_height);
        parser.setFont(fontName.toUtf8().toStdString(), fontSize);
        parser.setDuration(dm, ds);
        parser.setAlpha(alpha);

        // Convert
        MpvObject::instance()->addDanmaku(parser.convert());
    }
    m_reply->deleteLater();
    m_reply = nullptr;
}
