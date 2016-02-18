#
/*
 *    Copyright (C) 2014
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Programming
 *
 *    This file is part of the  SDR-J series.
 *    Many of the ideas as implemented in the SDR-J are derived from
 *    other work, made available through the (a) GNU general Public License. 
 *    All copyrights of the original authors are recognized.
 *
 *    SDR-J is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    SDR-J is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include	"fm-constants.h"
#include	"gui.h"
#include	"fm-processor.h"
#include	"fm-demodulator.h"
#include	"rds-decoder.h"
#include	"audiosink.h"
#include	"virtual-input.h"
#include	"dabstick-dll.h"

#ifdef __MINGW32__
#include	<iostream>
#include	<windows.h>
#endif

GST_DEBUG_CATEGORY_EXTERN (sdrjfm_debug);
#define GST_CAT_DEFAULT sdrjfm_debug

//
//	Processing modes
#define	IDLE		0100
#define	PAUSED		0101
#define	RUNNING		0102
#define	STOPPING	0103
/*
 *	We use the creation function merely to set up the
 *	user interface and make the connections between the
 *	gui elements and the handling agents. All real action
 *	is embedded in actions, initiated by gui buttons
 */
	RadioInterface::RadioInterface (int32_t frequency,
					ClearCallback		labelClearCallback,
					StringCallback	labelChangeCallback,
					StringCallback	labelCompleteCallback,
					ClearCallback		textClearCallback,
					StringCallback	textChangeCallback,
					StringCallback	textCompleteCallback,
					void *		callbackUserData): myFMprocessor(0) {
std::string h;
bool	success;

	runMode			= IDLE;
	squelchMode		= false;
//
//	not used - other than as dummy parameters - in the "mini"
	this		-> audioRate	= 44100;

	if (audioRate == 22050) {
	   this -> inputRate = 48 * audioRate;
	   this -> fmRate    = 8 * audioRate;
	}
	else
	if (audioRate == 44100) {
	   this -> inputRate = 24 * audioRate;
	   this -> fmRate    = 4  * audioRate;
	}
	else {
	   audioRate = 48000;
	   fmRate    = 192000;
	   inputRate = 960000;
	}

	myRig = new dabstick_dll (inputRate, &success);
	
	if (!success) {
	  // FIXME: Need new method of error reporting
	   exit (1);
	}
	
	setTuner (frequency);

	myFMprocessor		= NULL;
	our_audioSink		= new audioSink;

//
	audioDumping		= false;
	audiofilePointer	= NULL;

	systemClock             = gst_system_clock_obtain ();
	periodicClockId         = 0;
	resetSeekMembers ();

//
//
	currentPIcode		= 0;
	frequencyforPICode	= 0;
	int16_t	thresHold	= 20;
//
//	The FM processor is currently shared with the
//	regular FM software, so lots on dummy parameters
	myFMprocessor	= new fmProcessor  (myRig,
	                                    this,
	                                    our_audioSink,
	                                    inputRate,
	                                    fmRate,
	                                    this -> audioRate,
	                                    thresHold,
					    labelClearCallback,
					    labelChangeCallback,
					    labelCompleteCallback,
					    textClearCallback,
					    textChangeCallback,
					    textCompleteCallback,
					    callbackUserData);
}

	RadioInterface::~RadioInterface () {
	if (myFMprocessor != NULL)
	   delete myFMprocessor;
	gst_object_unref (GST_OBJECT (systemClock));
	delete		our_audioSink;
	delete myRig;
}

void	RadioInterface::start	(void) {
	if (runMode == RUNNING)
		return;

	myRig		-> restartReader ();
	myFMprocessor   -> start ();

	runMode = RUNNING;
}

void	RadioInterface::stop	(void) {
	if (runMode != RUNNING)
		return;

	// FIXME: This is a workaround for old libusb on Tizen
	//myRig		-> stopReader ();
	myFMprocessor	-> stop ();

	runMode = PAUSED;
}


/*
//
//	On start, we ensure that the streams are stopped so
//	that they can be restarted again.
void	RadioInterface::setStart	(void) {
bool	r = 0;

	if (runMode == RUNNING)
	   return;
//
//	always ensure that datastreams are stopped
	myRig		-> stopReader ();
//
	r = myRig		-> restartReader ();
	if (!r) {
	  // FIXME: Different notification method
	  //QMessageBox::warning (this, tr ("sdr"),
	  //tr ("Opening  input stream failed\n"));
	   return;
	}

//	and finally: recall that starting overrules pausing
	runMode	= RUNNING;
}
*/

void	RadioInterface::TerminateProcess (void) {
	runMode		= STOPPING;
	cancelSeek ();
	if (audioDumping) {
	   //sf_close	(audiofilePointer);
	}
//
//	It is pretty important that no one is attempting to
//	set things within the FMprocessor when it is
//	being deleted. Correct order is
//	stop the Reader first
	myRig		-> stopReader ();
	usleep (100);
	myFMprocessor	-> stop ();
	
	//qDebug () <<  "Termination started";
}

void	RadioInterface::abortSystem (int d) {
	//qDebug ("aborting for reason %d\n", d);
}
//
//
void	RadioInterface::setGainSelector (int g) {
	myRig	-> setExternalGain (g);
}

void	RadioInterface::setfmChannelSelector (const std::string &s) {
uint8_t	channelSelector;

	if (s == "stereo")
	   channelSelector	= fmProcessor::S_STEREO;
	else
	if (s == "Left")
	   channelSelector	= fmProcessor::S_LEFT;
	else
	if (s == "Right")
	   channelSelector	= fmProcessor::S_RIGHT;
	else
	if (s == "Left+Right")
	   channelSelector	= fmProcessor::S_LEFTplusRIGHT;
	else
	if (s == "Left-Right")
	   channelSelector	= fmProcessor::S_LEFTminusRIGHT;
	else		// the default
	   channelSelector	= fmProcessor::S_LEFT;
	if (myFMprocessor != NULL)
	   myFMprocessor	-> setSoundMode	(channelSelector);
}

void	RadioInterface::setAttenuation (int n) {
	if (myFMprocessor != NULL)
	   myFMprocessor	-> setAttenuation (2 * n);
}
//
//	The generic setTuner.
void	RadioInterface::setTuner (int32_t n) {
	myRig		-> setVFOFrequency		(n);
	if (myFMprocessor != NULL)
	   myFMprocessor	-> resetRds	();
}

void	RadioInterface::setFrequencyChangeCB (vfoFrequencyChangedCB cb, void *userData) {
	myRig		-> setVFOFrequencyChangeCallback	(cb, userData);
}
//
void	RadioInterface::seek (int16_t threshold,
			      int32_t minFrequency, int32_t maxFrequency,
			      int32_t frequencyStep, int32_t interval,
			      StationCallback callback, void *userdata) {
	if (myFMprocessor -> isScanning ()) {
		cancelSeekTimeout();
	}

	seekCallback	= callback;
	seekUserdata	= userdata;
	seekMin		= minFrequency;
	seekMax		= maxFrequency;
	seekStep	= frequencyStep;

	preSeekFrequency = myRig -> getVFOFrequency ();

	GST_DEBUG("Starting seek with threshold %d, minimum frequency %d Hz"
		  ", maximum frequency %d Hz, frequency step %d Hz"
		  ", interval %d milliseconds, pre-seek frequency %d Hz",
		  threshold, seekMin, seekMax, seekStep, interval, preSeekFrequency);

	iterateSeekFrequency ();

	myFMprocessor -> startScanning (&RadioInterface::stationCallback, this, threshold);

	periodicClockId	= gst_clock_new_periodic_id (systemClock,
						     gst_clock_get_time(systemClock),
						     interval * 1000000);

	gst_clock_id_wait_async (periodicClockId, &RadioInterface::seekTimeout, this, NULL);
}

void	RadioInterface::cancelSeek () {
	if (!myFMprocessor -> isScanning ())
		return;

	cancelSeekTimeout ();
	myFMprocessor -> stopScanning ();
	myRig -> setVFOFrequency (preSeekFrequency);

	resetSeekMembers();
}

void	RadioInterface::iterateSeekFrequency() {
	int32_t nextFrequency = myRig -> getVFOFrequency() + seekStep;
	if (nextFrequency > seekMax)
		nextFrequency = seekMin;
	else if (nextFrequency < seekMin)
		nextFrequency = seekMax;

	myRig -> setVFOFrequency (nextFrequency);
}

void	RadioInterface::cancelSeekTimeout() {
	gst_clock_id_unschedule (periodicClockId);
	gst_clock_id_unref (periodicClockId);
	periodicClockId         = 0;
}

void	RadioInterface::resetSeekMembers() {
	preSeekFrequency = seekMin = seekMax = -1;
	seekStep                = 0;
	seekUserdata            = 0;
	seekCallback            = 0;
}

void	RadioInterface::stationCallback(int32_t frequency, void *userdata) {
	static_cast<RadioInterface *>(userdata)->stationCallback(frequency);
}

void	RadioInterface::stationCallback(int32_t frequency) {
	StationCallback cb = seekCallback;
	void *userdata = seekUserdata;

	GST_DEBUG("Station found, cancelling seek timeout...");
	cancelSeekTimeout ();
	GST_DEBUG("...seek timeout cancelled");
	resetSeekMembers ();

	cb(frequency, userdata);
}

gboolean RadioInterface::seekTimeout (GstClock *clock, GstClockTime time,
				      GstClockID id, gpointer user_data) {
	return static_cast<RadioInterface *>(user_data)->seekTimeout();
}

gboolean RadioInterface::seekTimeout () {
	if (!myFMprocessor -> isScanning ()) {
		GST_DEBUG("Seek timeout while not scanning; returning with final");
		return FALSE;
	}

	GST_DEBUG("Seek timeout, iterating frequency");
	iterateSeekFrequency ();

	return TRUE;
}
	
	
//	Deemphasis	= 50 usec (3183 Hz, Europe)
//	Deemphasis	= 75 usec (2122 Hz US)
void	RadioInterface::setfmDeemphasis	(const std::string& s) {
	if (myFMprocessor == NULL)
	   return;
	if (s == "50")
	   myFMprocessor	-> setDeemphasis (50);
	else
	if (s == "75")
	   myFMprocessor	-> setDeemphasis (75);
	else
	   myFMprocessor	-> setDeemphasis (1);
}

void	RadioInterface::setVolume (int v) {
	if (myFMprocessor != NULL)
	   myFMprocessor	-> setVolume ((int16_t)v);
}
//

/*
void	RadioInterface::setCRCErrors	(int n) {
//	crcErrors	-> display (n);
	(void)n;
}

void	RadioInterface::setSyncErrors	(int n) {
//	syncErrors	-> display (n);
	(void)n;
}

void	RadioInterface::setbitErrorRate	(double v) {
//	bitErrorRate	-> display (v);
	(void)v;
}

void	RadioInterface::setGroup	(int n) {
	(void)n;
//	rdsGroupDisplay	-> display (n);
}

void	RadioInterface::setPTYCode	(int n) {
	(void)n;
//	rdsPTYDisplay	-> display (n);
}

void	RadioInterface::setPiCode	(int n) {
int32_t	t	= myRig -> getVFOFrequency ();

	if ((frequencyforPICode != t) || (n != 0)) {
	   currentPIcode	= n;
	   frequencyforPICode = t;
	}
}
*/

void	RadioInterface::setfmMode (const std::string &s) {
	myFMprocessor	-> setfmMode (s == "stereo");
}

void	RadioInterface::setfmRdsSelector (const std::string &s) {
	rdsModus = (s == "rds 1" ? rdsDecoder::RDS1 :
	            s == "rds 2" ? rdsDecoder::RDS2 : 
	            rdsDecoder::NO_RDS);
	myFMprocessor	-> setfmRdsSelector (rdsModus);
}

void	RadioInterface::setfmDecoder (const std::string &s) {
int8_t	decoder	= 0;

	if (s == "fm1decoder")
	   decoder = fm_Demodulator::FM1DECODER;
	else
	if (s == "fm2decoder")
	   decoder = fm_Demodulator::FM2DECODER;
	else
	if (s == "fm3decoder")
	   decoder = fm_Demodulator::FM3DECODER;
	else
	if (s == "fm4decoder")
	   decoder = fm_Demodulator::FM4DECODER;
	else
	   decoder = fm_Demodulator::FM5DECODER;

	myFMprocessor	-> setFMdecoder (decoder);
}

////////////////////////////////////////////////////////////////////

void	RadioInterface::set_squelchMode	(void) {
	if (myFMprocessor == NULL)
	   return;
	squelchMode = !squelchMode;
	myFMprocessor -> set_squelchMode (squelchMode);
}

void	RadioInterface::set_squelchValue (int n) {
	if (myFMprocessor != NULL)
	   myFMprocessor -> set_squelchValue (n);
}

int32_t RadioInterface::getSamples (DSPFLOAT *data, uint32_t length) {
	if (getWaitingSamples () == 0 && runMode != RUNNING)
		return -1;

	return our_audioSink -> getSamples (data, length);
}

uint32_t RadioInterface::getWaitingSamples () {
	return our_audioSink -> waiting ();
}

void	RadioInterface::cancelGet (void) {
	GST_DEBUG("Cancelling get in audio sink");
	our_audioSink -> cancelGet ();
	GST_DEBUG("Audio sink get cancelled");
}

void	RadioInterface::flush (void) {
	our_audioSink -> flush ();
}
