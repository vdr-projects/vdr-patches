/*
 * dvbdevice.h: The DVB device interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbdevice.h 1.14 2002/10/12 11:15:39 kls Exp $
 */

#ifndef __DVBDEVICE_H
#define __DVBDEVICE_H

#ifdef NEWSTRUCT
#include <linux/dvb/frontend.h>
#else
#include <ost/frontend.h>
#endif
#include "device.h"
#include "dvbspu.h"
#include "eit.h"

#define MAXDVBDEVICES  4

class cDvbDevice : public cDevice {
  friend class cDvbOsd;
private:
  static bool Probe(const char *FileName);
         // Probes for existing DVB devices.
public:
  static bool Initialize(void);
         // Initializes the DVB devices.
         // Must be called before accessing any DVB functions.
private:
#ifdef NEWSTRUCT
  fe_type_t frontendType;
  int fd_osd, fd_frontend, fd_audio, fd_video, fd_dvr;
#else
  FrontendType frontendType;
  int fd_osd, fd_frontend, fd_sec, fd_audio, fd_video, fd_dvr;
#endif
  int OsdDeviceHandle(void) const { return fd_osd; }
protected:
  virtual void MakePrimaryDevice(bool On);
public:
  cDvbDevice(int n);
  virtual ~cDvbDevice();
  virtual bool HasDecoder(void) const;

// OSD facilities

private:
  cDvbSpuDecoder *spuDecoder;
public:
  cOsdBase *NewOsd(int x, int y);
  virtual cSpuDecoder *GetSpuDecoder(void);

// Channel facilities

private:
  int source;
  int frequency;
  const char *diseqcCommands;
  bool IsTunedTo(const cChannel *Channel) const;
public:
  virtual bool ProvidesSource(int Source) const;
  virtual bool ProvidesChannel(const cChannel *Channel, int Priority = -1, bool *NeedsDetachReceivers = NULL) const;
protected:
  virtual bool SetChannelDevice(const cChannel *Channel, bool LiveView);

// PID handle facilities

protected:
  virtual bool SetPid(cPidHandle *Handle, int Type, bool On);

// Image Grab facilities

public:
  virtual bool GrabImage(const char *FileName, bool Jpeg = true, int Quality = -1, int SizeX = -1, int SizeY = -1);

// Video format facilities

public:
  virtual void SetVideoFormat(bool VideoFormat16_9);

// Audio facilities

private:
  int aPid1, aPid2;
protected:
  virtual void SetVolumeDevice(int Volume);
  virtual int NumAudioTracksDevice(void) const;
  virtual const char **GetAudioTracksDevice(int *CurrentTrack = NULL) const;
  virtual void SetAudioTrackDevice(int Index);

// EIT facilities

private:
  cSIProcessor *siProcessor;

// Player facilities

protected:
  ePlayMode playMode;
  virtual bool SetPlayMode(ePlayMode PlayMode);
public:
  virtual void TrickSpeed(int Speed);
  virtual void Clear(void);
  virtual void Play(void);
  virtual void Freeze(void);
  virtual void Mute(void);
  virtual void StillPicture(const uchar *Data, int Length);
  virtual bool Poll(cPoller &Poller, int TimeoutMs = 0);
  virtual int PlayVideo(const uchar *Data, int Length);
  virtual int PlayAudio(const uchar *Data, int Length);

// Receiver facilities

private:
  cTSBuffer *tsBuffer;
protected:
  virtual bool OpenDvr(void);
  virtual void CloseDvr(void);
  virtual bool GetTSPacket(uchar *&Data);
  };

#endif //__DVBDEVICE_H