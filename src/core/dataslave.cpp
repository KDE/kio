/*
 *  This file is part of the KDE libraries
 *  Copyright (c) 2003 Leo Savernik <l.savernik@aon.at>
 *  Derived from slave.cpp
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 **/

#include "dataslave_p.h"
#include "dataprotocol_p.h"
#include "commands_p.h"
#include "slavebase.h"

#include <klocalizedstring.h>

#include <QtCore/QTimer>

using namespace KIO;

#define KIO_DATA_POLL_INTERVAL 0

// don't forget to sync DISPATCH_DECL in dataslave_p.h
#define DISPATCH_IMPL(type) \
	void DataSlave::dispatch_##type() { \
	  if (_suspended) { \
	    QueueStruct q(Queue_##type); \
	    q.size = -1; \
	    dispatchQueue.push_back(q); \
	    if (!timer->isActive()) timer->start(KIO_DATA_POLL_INTERVAL); \
	  } else \
	    type(); \
	}

// don't forget to sync DISPATCH_DECL1 in dataslave_p.h
#define DISPATCH_IMPL1(type, paramtype, paramname) \
	void DataSlave::dispatch_##type(paramtype paramname) { \
	  if (_suspended) { \
	    QueueStruct q(Queue_##type); \
	    q.paramname = paramname; \
	    dispatchQueue.push_back(q); \
	    if (!timer->isActive()) timer->start(KIO_DATA_POLL_INTERVAL); \
	  } else \
	    type(paramname); \
	}


DataSlave::DataSlave() :
	Slave("data")
{
  //qDebug() << this;
  _suspended = false;
  timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), SLOT(dispatchNext()));
}

DataSlave::~DataSlave() {
  //qDebug() << this;
}

void DataSlave::hold(const QUrl &/*url*/) {
  // ignored
}

void DataSlave::suspend() {
  _suspended = true;
  //qDebug() << this;
  timer->stop();
}

void DataSlave::resume() {
  _suspended = false;
  //qDebug() << this;
  // aarrrgh! This makes the once hyper fast and efficient data protocol
  // implementation slow as molasses. But it wouldn't work otherwise,
  // and I don't want to start messing around with threads
  timer->start(KIO_DATA_POLL_INTERVAL);
}

// finished is a special case. If we emit it right away, then
// TransferJob::start can delete the job even before the end of the method
void DataSlave::dispatch_finished() {
    QueueStruct q(Queue_finished);
    q.size = -1;
    dispatchQueue.push_back(q);
    if (!timer->isActive()) timer->start(KIO_DATA_POLL_INTERVAL);
}

void DataSlave::dispatchNext() {
  if (dispatchQueue.empty()) {
    timer->stop();
    return;
  }

  const QueueStruct &q = dispatchQueue.front();
  //qDebug() << this << "dispatching" << q.type << dispatchQueue.size() << "left";
  switch (q.type) {
    case Queue_mimeType:	mimeType(q.s); break;
    case Queue_totalSize:	totalSize(q.size); break;
    case Queue_sendMetaData:	sendMetaData(); break;
    case Queue_data:		data(q.ba); break;
    case Queue_finished:	finished(); break;
  }/*end switch*/

  dispatchQueue.pop_front();
}

void DataSlave::send(int cmd, const QByteArray &arr) {
  QDataStream stream(arr);

  QUrl url;

  switch (cmd) {
    case CMD_GET: {
      stream >> url;
      get(url);
      break;
    }
    case CMD_MIMETYPE: {
      stream >> url;
      mimetype(url);
      break;
    }
    // ignore these (must not emit error, otherwise SIGSEGV occurs)
    case CMD_REPARSECONFIGURATION:
    case CMD_META_DATA:
    case CMD_SUBURL:
      break;
    default:
      error(ERR_UNSUPPORTED_ACTION,
		unsupportedActionErrorString(QLatin1String("data"),cmd));
  }/*end switch*/
}

bool DataSlave::suspended() {
  return _suspended;
}

void DataSlave::setHost(const QString &/*host*/, quint16 /*port*/,
                     const QString &/*user*/, const QString &/*passwd*/) {
  // irrelevant -> will be ignored
}

void DataSlave::setConfig(const MetaData &/*config*/) {
  // FIXME: decide to handle this directly or not at all
#if 0
    QByteArray data;
    QDataStream stream( data, QIODevice::WriteOnly );
    stream << config;
    slaveconn.send( CMD_CONFIG, data );
#endif
}

void DataSlave::setAllMetaData(const MetaData &md) {
  meta_data = md;
}

void DataSlave::sendMetaData() {
  emit metaData(meta_data);
}

DISPATCH_IMPL1(mimeType, const QString &, s)
DISPATCH_IMPL1(totalSize, KIO::filesize_t, size)
DISPATCH_IMPL(sendMetaData)
DISPATCH_IMPL1(data, const QByteArray &, ba)

#undef DISPATCH_IMPL
#undef DISPATCH_IMPL1

