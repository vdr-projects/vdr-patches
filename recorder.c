/*
 * recorder.c: The actual DVB recorder
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: recorder.c 2.4 2009/05/23 12:18:25 kls Exp $
 */

#include "recorder.h"
#include "shutdown.h"

#define RECORDERBUFSIZE  MEGABYTE(5)

// The maximum time we wait before assuming that a recorded video data stream
// is broken:
#define MAXBROKENTIMEOUT 30 // seconds

#define MINFREEDISKSPACE    (512) // MB
#define DISKCHECKINTERVAL   100 // seconds

// --- cRecorder -------------------------------------------------------------

cRecorder::cRecorder(const char *FileName, tChannelID ChannelID, int Priority, int VPid, const int *APids, const int *DPids, const int *SPids, const int *EPids)
:cReceiver(ChannelID, Priority, VPid, APids, Setup.UseDolbyDigital ? DPids : NULL, SPids, EPids)
,cThread("recording")
,recordingInfo(FileName)
{
  // Make sure the disk is up and running:

  SpinUpDisk(FileName);

  ringBuffer = new cRingBufferLinear(RECORDERBUFSIZE, TS_SIZE * 2, true, "Recorder");
  ringBuffer->SetTimeouts(0, 100);
  cChannel *Channel = Channels.GetByChannelID(ChannelID);
  int Pid = VPid;
  int Type = Channel ? Channel->Vtype() : 0;
  if (!Pid && APids) {
     Pid = APids[0];
     Type = 0x04;
     }
  if (!Pid && DPids) {
     Pid = DPids[0];
     Type = 0x06;
     }
  frameDetector = new cFrameDetector(Pid, Type);
  index = NULL;
  fileSize = 0;
  lastDiskSpaceCheck = time(NULL);
  fileName = new cFileName(FileName, true);
  int PatVersion, PmtVersion;
  if (fileName->GetLastPatPmtVersions(PatVersion, PmtVersion))
     patPmtGenerator.SetVersions(PatVersion + 1, PmtVersion + 1);
  patPmtGenerator.SetChannel(Channel);
  recordFile = fileName->Open();
  if (!recordFile)
     return;
  // Create the index file:
  index = new cIndexFile(FileName, true);
  if (!index)
     esyslog("ERROR: can't allocate index");
     // let's continue without index, so we'll at least have the recording
}

cRecorder::~cRecorder()
{
  Detach();
  delete index;
  delete fileName;
  delete frameDetector;
  delete ringBuffer;
}

bool cRecorder::RunningLowOnDiskSpace(void)
{
  if (time(NULL) > lastDiskSpaceCheck + DISKCHECKINTERVAL) {
     int Free = FreeDiskSpaceMB(fileName->Name());
     lastDiskSpaceCheck = time(NULL);
     if (Free < MINFREEDISKSPACE) {
        dsyslog("low disk space (%d MB, limit is %d MB)", Free, MINFREEDISKSPACE);
        return true;
        }
     }
  return false;
}

bool cRecorder::NextFile(void)
{
  if (recordFile && frameDetector->IndependentFrame()) { // every file shall start with an independent frame
     if (fileSize > MEGABYTE(off_t(Setup.MaxVideoFileSize)) || RunningLowOnDiskSpace()) {
        recordFile = fileName->NextFile();
        fileSize = 0;
        }
     }
  return recordFile != NULL;
}

void cRecorder::Activate(bool On)
{
  if (On)
     Start();
  else
     Cancel(3);
}

void cRecorder::Receive(uchar *Data, int Length)
{
  if (Running()) {
     int p = ringBuffer->Put(Data, Length);
     if (p != Length && Running())
        ringBuffer->ReportOverflow(Length - p);
     }
}

void cRecorder::Action(void)
{
  time_t t = time(NULL);
  bool InfoWritten = false;
  while (Running()) {
        int r;
        uchar *b = ringBuffer->Get(r);
        if (b) {
           int Count = frameDetector->Analyze(b, r);
           if (Count) {
              if (!Running() && frameDetector->IndependentFrame()) // finish the recording before the next independent frame
                 break;
              if (frameDetector->Synced()) {
                 if (!InfoWritten) {
                    if (recordingInfo.Read()) {
                       if (frameDetector->FramesPerSecond() > 0 && recordingInfo.FramesPerSecond() != frameDetector->FramesPerSecond()) {
                          recordingInfo.SetFramesPerSecond(frameDetector->FramesPerSecond());
                          recordingInfo.Write();
                          }
                       }
                    InfoWritten = true;
                    }
                 if (!NextFile())
                    break;
                 if (index && frameDetector->NewFrame())
                    index->Write(frameDetector->IndependentFrame(), fileName->Number(), fileSize);
                 if (frameDetector->IndependentFrame()) {
                    recordFile->Write(patPmtGenerator.GetPat(), TS_SIZE);
                    fileSize += TS_SIZE;
                    int Index = 0;
                    while (uchar *pmt = patPmtGenerator.GetPmt(Index)) {
                          recordFile->Write(pmt, TS_SIZE);
                          fileSize += TS_SIZE;
                          }
                    }
                 if (recordFile->Write(b, Count) < 0) {
                    LOG_ERROR_STR(fileName->Name());
                    break;
                    }
                 fileSize += Count;
                 t = time(NULL);
                 }
              ringBuffer->Del(Count);
              }
           }
        if (time(NULL) - t > MAXBROKENTIMEOUT) {
           esyslog("ERROR: video data stream broken");
           ShutdownHandler.RequestEmergencyExit();
           t = time(NULL);
           }
        }
}
