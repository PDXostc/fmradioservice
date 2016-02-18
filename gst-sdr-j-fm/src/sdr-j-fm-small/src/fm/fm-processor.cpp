#
/*
 *    Copyright (C) 2010, 2011, 2012
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Programming
 *
 *    This file is part of the SDR-J program suite.
 *    Many of the ideas as implemented in SDR-J are derived from
 *    other work, made available through the GNU general Public License. 
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
 *    along with SDR-J; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include	"fm-processor.h"
#include	"gui.h"
#include	"fm-demodulator.h"
#include	"fm-constants.h"
#include	"rds-decoder.h"
#include	"audiosink.h"
#include	"squelchClass.h"
#include	"sincos.h"
#include	"virtual-input.h"
#include	"newconverter.h"
#include	<stdexcept>
#include	<iostream>

GST_DEBUG_CATEGORY_EXTERN (sdrjfm_debug);
#define GST_CAT_DEFAULT sdrjfm_debug

#define	AUDIO_FREQ_DEV_PROPORTION 0.85f
#define	PILOT_FREQUENCY		19000
#define	RDS_FREQUENCY		(3 * PILOT_FREQUENCY)
#define	OMEGA_DEMOD		2 * M_PI / fmRate
#define	OMEGA_PILOT	((DSPFLOAT (PILOT_FREQUENCY)) / fmRate) * (2 * M_PI)
#define	OMEGA_RDS	((DSPFLOAT) RDS_FREQUENCY / fmRate) * (2 * M_PI)

//
//	Note that no decimation done as yet: the samplestream is still
//	full speed
	fmProcessor::fmProcessor (virtualInput		*vi,
	                          RadioInterface	*RI,
	                          audioSink		*mySink,
	                          int32_t		inputRate,
	                          int32_t		fmRate,
	                          int32_t		audioRate,
	                          int16_t		thresHold,
				  ClearCallback		labelClearCallback,
				  StringCallback	labelChangeCallback,
				  StringCallback	labelCompleteCallback,
				  ClearCallback		textClearCallback,
				  StringCallback	textChangeCallback,
				  StringCallback	textCompleteCallback,
				  void *		callbackUserData) {
	running				= false;
	this	-> myRig		= vi;
	this	-> myRadioInterface	= RI;
	this	-> theSink		= mySink;
	this	-> inputRate		= inputRate;
	this	-> fmRate		= fmRate;
	this	-> decimatingScale	= inputRate / fmRate;
	this	-> audioRate		= audioRate;
	this	-> thresHold		= thresHold;
	this	-> squelchOn		= false;

	pthread_mutex_init (&this -> scanLock, NULL);
	this	-> scanning		= false;
  	this	-> scan_fft		= new common_fft (1024);
  	this	-> scanPointer		= 0;

	this	-> localOscillator	= new Oscillator (inputRate);
	this	-> mySinCos		= new SinCos (fmRate);
	this	-> lo_frequency		= 0;
	this	-> omega_demod		= 2 * M_PI / fmRate;
/*
 *	default values, will be set through the user interface
 *	to their appropriate values
 */
	this	-> fmModus		= FM_STEREO;
	this	-> selector		= S_STEREO;
	this	-> Gain			= 50;
	this	-> Volume		= 10;
	this	-> inputMode		= IandQ;
#define	RDS_DECIMATOR	8
	this	-> myRdsDecoder		= new rdsDecoder (myRadioInterface,
	                                                  fmRate / RDS_DECIMATOR,
	                                                  mySinCos,
							  labelClearCallback,
							  labelChangeCallback,
							  labelCompleteCallback,
							  textClearCallback,
							  textChangeCallback,
							  textCompleteCallback,
							  callbackUserData);
	this	-> rdsModus		= rdsDecoder::RDS2;
/*
 *	averagePeakLevel and audioGain are set
 *	prior to calling the processFM method
 */
	this	-> peakLevel		= -100;
	this	-> peakLevelcnt		= 0;
	this	-> max_freq_deviation	= 0.95 * (0.5 * fmRate);
	this	-> norm_freq_deviation	= 0.7 * max_freq_deviation;
	this	-> audioGain		= 0;
//
	this	-> fm_Levels		= new fmLevels (LEVEL_SIZE,
	                                                fmRate, LEVEL_FREQ);
//
//	Since data is coming with a pretty high rate, we need to filter
//	and decimate in an efficient way. We have an optimized
//	decimating filter
	fmBandfilter		= new DecimatingFIR (15,
	                                             fmRate / 2,
	                                             inputRate,
	                                             decimatingScale);
//
//	to isolate the pilot signal, we need a reasonable
//	filter. The filtered signal is beautified by a pll
	pilotBandFilter		= new fftFilter (FFT_SIZE, PILOTFILTER_SIZE);
	pilotBandFilter		-> setBand (PILOT_FREQUENCY - PILOT_WIDTH / 2,
		                            PILOT_FREQUENCY + PILOT_WIDTH / 2,
		                            fmRate);
	pilotRecover		= new pilotRecovery (fmRate,
	                                             OMEGA_PILOT,
	                                             25 * omega_demod,
	                                             mySinCos);
	pilotDelay	= (FFT_SIZE - PILOTFILTER_SIZE) * OMEGA_PILOT;

//	rdsLowPassFilter	= new fftFilter (FFT_SIZE, RDSLOWPASS_SIZE);
//	rdsLowPassFilter	-> setLowPass (RDS_WIDTH, fmRate);
	rdsLowPassFilter	= new DecimatingFIR (21,
	                                             RDS_WIDTH / 2,
	                                             fmRate,
	                                             RDS_DECIMATOR);
//
//	the constant K_FM is still subject to many questions
	DSPFLOAT	F_G	= 60000;	// highest freq in message
	DSPFLOAT	Delta_F	= 90000;	//
	DSPFLOAT	B_FM	= 2 * (Delta_F + F_G);
	K_FM			= 2 * M_PI / fmRate * (B_FM / 2 - F_G);
	K_FM			= 4 * K_FM;

	TheDemodulator		= new fm_Demodulator (fmRate,
	                                              mySinCos, K_FM);
	fmAudioFilter		= new LowPassFIR (11, 11000, fmRate);
//
//	In the case of mono we do not assume a pilot
//	to be available. We borrow the approach from CuteSDR
	rdsHilbertFilter	= new HilbertFilter (HILBERT_SIZE,
	                                     (DSPFLOAT)RDS_FREQUENCY / fmRate,
	                                             fmRate);
	rdsBandFilter		= new fftFilter (FFT_SIZE,
	                                         RDSBANDFILTER_SIZE);
	rdsBandFilter		-> setSimple (RDS_FREQUENCY - RDS_WIDTH / 2,
	                                      RDS_FREQUENCY + RDS_WIDTH / 2,
	                                      fmRate);
	rds_plldecoder		= new pllC (fmRate,
	                                    RDS_FREQUENCY,
	                                    RDS_FREQUENCY - 50,
	                                    RDS_FREQUENCY + 50,
	                                    200,
	                                    mySinCos);

//	for the deemphasis we use an in-line filter with
	xkm1			= 0;
	ykm1			= 0;
	alpha			= 1.0 / (fmRate / (1000000.0 / 50.0 + 1));

	stopScanning();
	squelchValue		= 50;
	old_squelchValue	= 0;

	theConverter		= 0;
	myCount			= 0;

	setDeemphasis(50);
}

	fmProcessor::~fmProcessor (void) {
	if (running)
		stop	();

	delete	TheDemodulator;
	delete	rds_plldecoder;
	delete	pilotRecover;
	delete	rdsHilbertFilter;
	delete	rdsBandFilter;
	delete	pilotBandFilter;
	delete	fm_Levels;
	delete	mySinCos;
	delete fmAudioFilter;

	pthread_mutex_destroy (&this -> scanLock);
}

void *  fmProcessor::c_run (void * userdata) {
	fmProcessor *proc = static_cast<fmProcessor *>(userdata);
	proc->run();
	return NULL;
}

void	fmProcessor::start	(void) {
	int err = pthread_create(&threadId, NULL, &fmProcessor::c_run, this);
	if (err != 0) {
		std::ostringstream strm;
		strm << "error creating processor thread: "
		     << strerror(err);

		throw std::runtime_error(strm.str());
	}
}

void	fmProcessor::stop	(void) {
	running	= false;
	void *ret = NULL;
	int err = pthread_join(threadId, &ret);
	if (err != 0) {
		std::cerr << "warning: could not join processor thread: "
			  << strerror(err) << std::endl;
	} 
}

DSPFLOAT	fmProcessor::get_pilotStrength	(void) {
	if (running)
	   return fm_Levels	-> getPilotStrength ();
	return 0.0;
}

DSPFLOAT	fmProcessor::get_rdsStrength	(void) {
	if (running)
	   return fm_Levels	-> getRdsStrength ();
	return 0.0;
}

DSPFLOAT	fmProcessor::get_noiseStrength	(void) {
	if (running)

	   return fm_Levels	-> getNoiseStrength ();
	return 0.0;
}

void		fmProcessor::set_squelchValue	(int16_t n) {
	squelchValue	= n;
}

DSPFLOAT	fmProcessor::get_dcComponent	(void) {
	if (running)
	   return TheDemodulator	-> get_DcComponent ();
	return 0.0;
}

const char  *fmProcessor::nameofDecoder	(void) {
	if (running)
	   return TheDemodulator -> nameofDecoder ();
	return " ";
}
//
void	fmProcessor::setfmMode (uint8_t m) {
	fmModus	= m ? FM_STEREO : FM_MONO;
}

void	fmProcessor::setFMdecoder (int8_t d) {
	if (running)
	   TheDemodulator	-> setDecoder (d);
}

void	fmProcessor::setSoundMode (uint8_t selector) {
	this	-> selector = selector;
}

//	Deemphasis	= 50 usec (3183 Hz, Europe)
//	Deemphasis	= 75 usec (2122 Hz US)
//	tau		= 2 * M_PI * Freq = 1000000 / time
void	fmProcessor::setDeemphasis	(int16_t v) {
DSPFLOAT	Tau;

	switch (v) {
	   default:
	      v	= 1;
	   case	1:
	   case 50:
	   case 75:
//	pass the Tau
	      Tau	= 1000000.0 / v;
	      alpha	= 1.0 / (DSPFLOAT (fmRate) / Tau + 1.0);
	}
}

void	fmProcessor::setVolume (int16_t Vol) {
	Volume = Vol;
}

DSPCOMPLEX	fmProcessor::audioGainCorrection (DSPCOMPLEX z) {
	return cmul (z, audioGain * Volume);
	return z;
}

void	fmProcessor::setAttenuation (int16_t at) {
	Gain	= at;
}

void	fmProcessor::startScanning	(StationCallback callback, void *userdata,
					 int16_t threshold) {
	lockScan();
	if (threshold != -1)
		thresHold = threshold;

	scanning	= true;
	scanCallback    = callback;
	scanUserdata    = userdata;
	unlockScan();
}

void	fmProcessor::stopScanning	(void) {
	lockScan();
	scanning	= false;
	scanCallback    = 0;
	scanUserdata    = 0;
	unlockScan();
}

bool	fmProcessor::isScanning (void) {
	bool is;
	lockScan();
	is = scanning;
	unlockScan();
	return is;
}

void	fmProcessor::lockScan() {
	pthread_mutex_lock (&this -> scanLock);
}

void	fmProcessor::unlockScan() {
	pthread_mutex_unlock (&this -> scanLock);
}

bool	fmProcessor::checkStation(DSPCOMPLEX v) {
	bool ret = false;
	lockScan();
	if (scanning) {
	   ret = true;
	   DSPCOMPLEX *scanBuffer = scan_fft -> getVector ();
	   scanBuffer [scanPointer ++] = v;
	   if (scanPointer >= SCAN_BLOCK_SIZE) {
	      scanPointer	= 0;
	      scan_fft -> do_FFT ();
	      float signal	= get_db (getSignal	(scanBuffer, SCAN_BLOCK_SIZE), 256);
	      float noise	= get_db (getNoise	(scanBuffer, SCAN_BLOCK_SIZE), 256);
	      float ratio	= signal - noise;
	      GST_TRACE("SnR check, signal: %f, noise: %f, ratio: %f (threshold: %i)",
			      signal, noise, ratio, this -> thresHold);
	      if (ratio > this -> thresHold) {
		 GST_DEBUG("Station found; signal: %f, noise: %f, ratio: %f (threshold: %i)",
				 signal, noise, ratio, this -> thresHold);

		 addStation (ratio);

	      } else if (!stations.empty ()) {
		 finishScan ();
	      }
	   }
	}
	unlockScan();

	return ret;
}

void	fmProcessor::addStation(float ratio) {
	const int32_t frequency = myRig -> getVFOFrequency();

	if (stations.empty () || stations.back().frequency != frequency) {
	   StationData data;
	   data.frequency = frequency;
	   data.snr = ratio;
	   data.count = 1;
	   stations . push_back (data);
	} else {
	   StationData &existing = stations.back ();
	   existing.snr += ratio;
	   ++existing.count;
	}
}

void	fmProcessor::finishScan() {

	const int size = stations.size();
	int ind = size/2;

	// Calculate average snr from accumulated values
	StationData &mid = stations[ind];
	mid.snr /= mid.count;

	// If there's even size, there's two middle frequencies, pick the one
	// with the best signal-to-noise ratio
	if (size % 2 == 0) {
	   StationData &first = stations[ind-1];
	   first.snr /= first.count;
	   
	   if (first.snr > mid.snr)
		   ind -= 1;

	   GST_DEBUG("Choosing station with highest ratio from middle 2 out of %i", size);
	} else {
	   GST_DEBUG("Choosing middle station out of %i", size);
	} 

	StationData &data = stations[ind];

	scanCallback(data.frequency, scanUserdata);
	stations.clear();
	scanning = false;
}

void	fmProcessor::run (void) {
DSPCOMPLEX	result;
DSPFLOAT 	rdsData;
int32_t		bufferSize	= 16384;
DSPCOMPLEX	dataBuffer [bufferSize];
int32_t		i;
DSPCOMPLEX	out;
DSPCOMPLEX	pcmSample;
int32_t		a;
squelch		mySquelch (1, audioRate / 10, audioRate / 20, audioRate); 
float		audioGainAverage	= 0;
bool		pilotExists;
DSPCOMPLEX	pcmSamples [256];
int16_t		audioIndex	= 0;

	running	= true;		// will be set elsewhere

	while (running) {
	   int32_t x = bufferSize - myRig -> Samples ();
	   while ((x > 0) && running) {
//	we need x samples more than we have, before going on,
//	so with an inputRate "inputRate"
//	we wait (note that we need 64 bit computation
	      usleep (5000);	// should be enough
	      x =  bufferSize - myRig -> Samples ();
	   }

	   if (!running)
	      break;
//
	   if (squelchValue != old_squelchValue) {
	      mySquelch. setSquelchLevel (squelchValue);
	      old_squelchValue = squelchValue;
	   }
	
	   bufferSize = a =
	            myRig -> getSamples (dataBuffer, bufferSize, inputMode);

	   //std::cerr << now() << " got " << bufferSize << " samples from dongle" << std::endl;

//
//	Here we really start
//
//	We assume that if/when the pilot is no more than 3 db's above
//	the noise around it, it is better to decode mono
	   pilotExists	= fm_Levels -> getPilotStrength () > 3;
	   for (i = 0; i < bufferSize; i ++) {
	      DSPCOMPLEX v = dataBuffer [i];
//		first step: decimating, filtering and attenuation
	      if (!fmBandfilter -> Pass (v, &v))
	         continue;

	      v = v * DSPFLOAT (Gain);
//	second step: if we are scanning, do the scan
	      if (checkStation (v))
		 v = 0;

//	Now we have the signal ready for decoding
//	keep track of the peaklevel, we take segments
	      if (abs (v) > peakLevel)
	         peakLevel = abs (v);
	      if (++peakLevelcnt >= fmRate / 2) {
	         DSPFLOAT	ratio	= 
	                          max_freq_deviation / norm_freq_deviation;
	         if (peakLevel > 0)
	            this -> audioGain	= 
	                  (ratio / peakLevel) / AUDIO_FREQ_DEV_PROPORTION;
	         if (audioGain <= 0.1)
	            audioGain = 0.1;
	         audioGain	= 0.8 * audioGainAverage + 0.2 * audioGain;
	         audioGainAverage = audioGain;
	         peakLevelcnt	= 0;
	         peakLevel	= -100;
	      }

	      if ((fmModus == FM_STEREO) && pilotExists) {
	         stereo (&v, &result, &rdsData);
	         result = audioGainCorrection (result);
	         result = fmAudioFilter -> Pass (result);
//
//	for reasons of efficiency, we decimate inline
	         static int a_cnt = 0;
	         if (++a_cnt >= fmRate / audioRate) {
	            a_cnt = 0;
	            if (squelchOn)
	               result = mySquelch. do_squelch (result);
	            switch (selector) {
	               default:
	               case S_STEREO:
	                  result = DSPCOMPLEX (real (result) + imag (result),
	                                  - (- real (result) + imag (result)));
	                  break;

	               case S_LEFT:
	                  result = DSPCOMPLEX (real (result) + imag (result), 
	                                    real (result) + imag (result));
	                  break;

	               case S_RIGHT:
	                  result = DSPCOMPLEX (- (imag (result) - real (result)),
	                                    - (imag (result) - real (result)));
	                  break;

	               case S_LEFTplusRIGHT:
	                  result = DSPCOMPLEX (real (result),  real (result));
	                  break;

	               case S_LEFTminusRIGHT:
	                  result = DSPCOMPLEX (imag (result), imag (result));
	                  break;
	            }
	            pcmSamples [audioIndex ++] = result;
	            if (audioIndex >= 256) {
	               theSink	-> putSamples (pcmSamples, 256);
	               audioIndex = 0;
	            }
	         }
	      }
	      else {
	         mono (&v, &result, &rdsData);
	         result = audioGainCorrection (result);
	         result = fmAudioFilter -> Pass (result);

	         static int b_cnt = 0;
	         if (++b_cnt >= fmRate / audioRate) {
	            b_cnt = 0;
	            if (squelchOn)
	               result = mySquelch. do_squelch (result);
	            pcmSamples [audioIndex ++] = result;
	            if (audioIndex >= 256) {
	               theSink	-> putSamples (pcmSamples, 256);
	               audioIndex = 0;
	            }
	         }
	      }
	      if ((rdsModus != rdsDecoder::NO_RDS)) {
	         DSPFLOAT mag;
	         if (rdsLowPassFilter -> Pass (rdsData, &rdsData)) 
	            myRdsDecoder -> doDecode (rdsData, &mag,
	                                      (rdsDecoder::RdsMode)rdsModus);
	      }

	   }
	}
}

void	fmProcessor::mono (DSPCOMPLEX	*in,
	                   DSPCOMPLEX	*audioOut,
	                   DSPFLOAT	*rdsValue) {
DSPFLOAT	Re, Im;
DSPCOMPLEX	rdsBase;
DSPFLOAT	demod	= TheDemodulator	-> demodulate (*in);

	fm_Levels	-> addItem (demod);
//	deemphasize
	Re	= xkm1 = (demod - xkm1) * alpha + xkm1;
	Im	= ykm1 = (demod - ykm1) * alpha + ykm1;
	*audioOut	= DSPCOMPLEX (Re, Im);

	
	if ((rdsModus != rdsDecoder::NO_RDS)) {
//	    fully inspired by cuteSDR, we try to decode the rds stream
//	    by simply am decoding it (after creating a decent complex
//	    signal by Hilbert filtering)
	   rdsBase	= DSPCOMPLEX (5 * demod, 5 * demod);
	   rdsBase = rdsHilbertFilter -> Pass (rdsBandFilter -> Pass (rdsBase));
	   rds_plldecoder -> do_pll (rdsBase);
	   DSPFLOAT rdsDelay = imag (rds_plldecoder -> getDelay ());
//	   *rdsValue = rdsLowPassFilter -> Pass (5 * rdsDelay);
	   *rdsValue = 5 * rdsDelay;
	}
}

void	fmProcessor::stereo (DSPCOMPLEX	*in,
	                     DSPCOMPLEX	*audioOut,
	                     DSPFLOAT	*rdsValue) {

DSPFLOAT	LRPlus	= 0;
DSPFLOAT	LRDiff	= 0;
DSPFLOAT	pilot	= 0;
DSPFLOAT	currentPilotPhase;
DSPFLOAT	PhaseforLRDiff	= 0;
DSPFLOAT	PhaseforRds	= 0;
/*
 */
	DSPFLOAT demod	= TheDemodulator  -> demodulate (*in);
	fm_Levels	-> addItem (demod);
	LRPlus = LRDiff = pilot	= demod;
/*
 *	get the phase for the "carrier to be inserted" right
 */
	pilot		= pilotBandFilter -> Pass (5 * pilot);
	currentPilotPhase = pilotRecover -> getPilotPhase (5 * pilot);
/*
 *	Now we have the right - i.e. synchronized - signal to work with
 */
	PhaseforLRDiff	= 2 * (currentPilotPhase + pilotDelay);
	PhaseforRds	= 3 * (currentPilotPhase + pilotDelay);

	LRDiff	= 6 * mySinCos	-> getCos (PhaseforLRDiff) * LRDiff;
//
//	and for the RDS
	if ((rdsModus != rdsDecoder::NO_RDS)) {
	   DSPFLOAT  MixerValue = mySinCos -> getCos (PhaseforRds);
	   *rdsValue = 5 * MixerValue * demod;
//	   *rdsValue = 5 * rdsLowPassFilter -> Pass (MixerValue * demod);
	}

//	apply deemphasis
	LRPlus		= xkm1	= (LRPlus - xkm1) * alpha + xkm1;
	LRDiff		= ykm1	= (LRDiff - ykm1) * alpha + ykm1;
        *audioOut		= DSPCOMPLEX (LRPlus, LRDiff);
}

void	fmProcessor::setLFcutoff (int32_t Hz) {
	return;
	if (fmAudioFilter != NULL)
	   delete	fmAudioFilter;
	fmAudioFilter	= NULL;
	if (Hz > 0)
	   fmAudioFilter	= new LowPassFIR (11, Hz, fmRate);
}

bool	fmProcessor::isLocked (void) {
	if (!running)
	   return false;
	return ((fmModus == FM_STEREO) && (pilotRecover -> isLocked ()));
}


void	fmProcessor::setfmRdsSelector (int8_t m) {
	rdsModus	= m;
}

void	fmProcessor::resetRds	(void) {
	myRdsDecoder	-> reset ();
}

void	fmProcessor::set_LocalOscillator (int32_t lo) {
	lo_frequency = lo;
}

bool	fmProcessor::ok		(void) {
	return running;
}

void	fmProcessor::set_squelchMode (bool b) {
	squelchOn	= b;
}

void	fmProcessor::setInputMode	(uint8_t m) {
	inputMode	= m;
}

DSPFLOAT	fmProcessor::getSignal		(DSPCOMPLEX *v, int32_t size) {
DSPFLOAT sum = 0;
int16_t	i;

	for (i = 5; i < 25; i ++) {
	   sum += abs (v [i]);
	   sum += abs (v [size - 1 - i]);
	}
	return sum / 40;
}

DSPFLOAT	fmProcessor::getNoise		(DSPCOMPLEX *v, int32_t size) {
DSPFLOAT sum	= 0;
int16_t	i;

	for (i = 5; i < 25; i ++) {
	   sum += abs (v [size / 2 - 1 - i]);
	   sum += abs (v [size / 2 + 1 + i]);
	}
	return sum / 40;
}

