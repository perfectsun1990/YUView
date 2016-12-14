/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2015  Institut für Nachrichtentechnik
*                       RWTH Aachen University, GERMANY
*
*   YUView is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   YUView is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with YUView.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef VIDEOCACHE_H
#define VIDEOCACHE_H

#include <QPointer>
#include <QQueue>
#include <QWidget>
#include "playlistTreeWidget.h"

class videoHandler;
class videoCache;

class videoCacheStatusWidget : public QWidget
{
  Q_OBJECT

public:
  videoCacheStatusWidget(QWidget *parent) : QWidget(parent) {cache = nullptr;}
  // Override the paint event
  virtual void paintEvent(QPaintEvent *event) Q_DECL_OVERRIDE;
  void setPlaylist (PlaylistTreeWidget *playlistWidget) { playlist = playlistWidget; }
  void setCache (videoCache *someCache) { cache = someCache; }

private:
  QPointer<PlaylistTreeWidget> playlist;
  QPointer<videoCache> cache;
};

class videoCache : public QObject
{
  Q_OBJECT

public:
  // The video cache interfaces with the playlist to see which items are going to be played next and the
  // playback controller to get the position in the video and the current state (playback/stop).
  videoCache(PlaylistTreeWidget *playlistTreeWidget, PlaybackController *playbackController, QObject *parent = 0);
  ~videoCache();

  // The user might have changed the settings. Update.
  void updateSettings();

  // Load the given frame of the given object. This also includes a queue with only one slot. If a frame is currently being
  // loaded, the next call will be saved and started as soon as the running loading request is done. If a request is waiting
  // and another one arrives, the waiting request will be discarded.
  void loadFrame(playlistItem *item, int frameIndex);

  unsigned int cacheRateInBytesPerMs;

private slots:

  // This signal is sent from the playlistTreeWidget if something changed (another item was selected ...)
  // The video Cache will then re-evaluate what to cache next and start the cache worker.
  void playlistChanged();

  // The cacheThread finished. If we requested the interruption, update the cache queue and restart.
  // If the thread finished by itself, push the next item into it or goto idle state if there is no more things
  // to cache
  void threadCachingFinished();

  // The interactiveWorker finished loading a frame
  void interactiveLoaderFinished();

  // An item is about to be deleted. If we are currently caching something (especially from this item),
  // abort that operation immediately.
  void itemAboutToBeDeleted(playlistItem*);

  // update the caching rate at the video cache controller every 1s
  void updateCachingRate(unsigned int cacheRate);

private:
  // A cache job. Has a pointer to a playlist item and a range of frames to be cached.
  struct cacheJob
  {
    cacheJob() {}
    cacheJob(playlistItem *item, indexRange range);
    QPointer<playlistItem> plItem;
    indexRange frameRange;
  };
  typedef QPair<QPointer<playlistItem>, int> plItemFrame;

  // Analyze the current situation and decide which items are to be cached next (in which order) and
  // which frames can be removed from the cache.
  void updateCacheQueue();
  // When the cache queue is updated, this function will start the background caching.
  void startCaching();

  QPointer<PlaylistTreeWidget> playlist;
  QPointer<PlaybackController> playback;

  // Is caching even enabled?
  bool cachingEnabled;
  // The queue of caching jobs that are scheduled
  QQueue<cacheJob> cacheQueue;
  // The queue with a list of frames/items that can be removed from the queue if necessary
  QQueue<plItemFrame> cacheDeQueue;
  // If a frame is removed can be determined by the following cache states:
  qint64 cacheLevelMax;
  qint64 cacheLevelCurrent;

  // Start the given number of worker threads (if caching is running, also new jobs will be pushed to the workers)
  void startWorkerThreads(int nrThreads);
  // If this number is > 0, the indicated number of threads will be deleted when a worker finishes (threadCachingFinished() is called)
  int deleteNrThreads;

  // Our tiny internal state machine for the worker
  enum workerStateEnum
  {
    workerIdle,         // The worker is idle. We can update the cacheQuene and go to workerRunning
    workerRunning,      // The worker is running. If it finishes by itself goto workerIdle. If an interrupt is requested, goto workerInterruptRequested.
    workerIntReqStop,   // The worker is running but an interrupt was requested. Next goto workerIdle.
    workerIntReqRestart // The worker is running but an interrupt was requested because the queue needs updating. If the worker finished, we will update the queue and goto workerRunning.
  };
  workerStateEnum workerState;
  
  // This list contains the items that are scheduled for deletion. All items in this list will be deleted (->deleteLate()) when caching of a frame of this item is done.
  QList<playlistItem*> itemsToDelete;

  // A simple QObject (to move to threads) that gets a pointer to a playlist item and loads a frame in that item.
  class loadingWorker;

  // A list of caching threads that process caching of frames in parallel in the background
  QList<loadingWorker*> cachingWorkerList;
  QList<QThread*> cachingThreadList;

  // One thread with a higher priority that performs interactive loading (if the user is the source of the request)
  loadingWorker *interactiveWorker;
  QThread       *interactiveWorkerThread;
  playlistItem  *interactiveItemQueued;
  int            interactiveItemQueued_Idx;

  // Get the next item and frame to cache from the queue and push it to the given worker.
  // Return false if there are no more jobs to be pushed.
  bool pushNextJobToThread(loadingWorker *worder);
  
  bool updateCacheQueueAndRestartWorker;
};

#endif // VIDEOCACHE_H
