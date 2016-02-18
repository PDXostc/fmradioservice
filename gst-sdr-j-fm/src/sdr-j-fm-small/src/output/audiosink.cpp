#
/*
 *    Copyright (C) 2011, 2012, 2013
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Programming
 *
 *    This file is part of the SDR-J.
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
 */

#include	"audiosink.h"
#include	<gst/gst.h>
#include	<stdexcept>
#include	<iostream>
#include	<sys/time.h>

GST_DEBUG_CATEGORY_EXTERN (sdrjfm_debug);
#define GST_CAT_DEFAULT sdrjfm_debug

/*
 *	The class is the sink for the data generated
 */
	audioSink::audioSink	() {
	_O_Buffer		= new RingBuffer<float>(2 * 32768);

	pthread_mutex_init (&lock, NULL);
	pthread_cond_init (&sig, NULL);
	cancelled		= false;
}

	audioSink::~audioSink	(void) {
	delete	_O_Buffer;
}

//
//	helper
static inline
int32_t	minimum (int32_t a, int32_t b) {
	return a > b ? b : a;
}
//
//	Just for my own curiosity I want to know to what degree
//	the buffer is filled
int32_t	audioSink::capacity	(void) {
	return _O_Buffer -> GetRingBufferWriteAvailable () / 2;
}
int32_t	audioSink::waiting	(void) {
	return _O_Buffer -> GetRingBufferReadAvailable () / 2;
}
//
//	putSample output comes from the FM receiver
int32_t	audioSink::putSample	(DSPCOMPLEX v) {
	return putSamples (&v, 1);
}

int32_t	audioSink::putSamples		(DSPCOMPLEX *V, int32_t n) {
float	*buffer = (float *)alloca (2 * n * sizeof (float));
int32_t	i;
int32_t	available = _O_Buffer -> GetRingBufferWriteAvailable ();

	if (2 * n > available)
	   n = (available / 2) & ~01;
	for (i = 0; i < n; i ++) {
	   buffer [2 * i] = real (V [i]);
	   buffer [2 * i + 1] = imag (V [i]);
	}

	//std::cerr << now() << " writing 2*" << n << " samples to ringbuffer with "
	//<< available << " available space" << std::endl;

	GST_TRACE("Writing %d samples to SDR-J RingBuffer with %d available space",
		  n * 2, available);
	

	_O_Buffer	-> putDataIntoBuffer (buffer, 2 * n);
	signal ();
	return n;
}

int32_t audioSink::getSamples (DSPFLOAT *data, uint32_t count) {
int32_t available = _O_Buffer -> GetRingBufferReadAvailable ();


	GST_TRACE("Reading %u samples from SDR-J RingBuffer with %d available samples",
		  count, available);

	if (available < 1) {
		GST_TRACE("No samples in SDR-J RingBuffer, waiting");
		int err = wait ();

		if (err == -1) {
			GST_TRACE("Wait for SDR-J RingBuffer samples was cancelled");
			return -1;
		} else {
			GST_TRACE("Finished waiting for samples, %d now available",
				  _O_Buffer -> GetRingBufferReadAvailable ()); 
		}
	}

	return _O_Buffer -> getDataFromBuffer (data, count);
}

void	audioSink::cancelGet (void) {
	GST_DEBUG("Cancelling get");
	signal (true);
	GST_DEBUG("Get cancelled");
}

void	audioSink::flush (void) {
	_O_Buffer -> FlushRingBuffer ();
	GST_DEBUG("SDR-J RingBuffer flushed");
}

int	audioSink::wait (int32_t secs) {
	struct timeval now;
	int err = gettimeofday (&now, NULL);
	if (err != 0) {
		std::ostringstream strm;
		strm << "error getting time of day: " << strerror(errno); 
		throw std::runtime_error(strm.str());
	}

	struct timespec timeout;
	timeout.tv_sec = now.tv_sec + secs;
	timeout.tv_nsec = now.tv_usec * 1000;

	int ret = 0;


	pthread_mutex_lock (&lock);

	cancelled = false;
	while (!cancelled && _O_Buffer -> GetRingBufferReadAvailable () < 1) {
	        err = pthread_cond_timedwait (&sig, &lock, &timeout);
		if (err == ETIMEDOUT) { 
			ret = -2;
			break;
		}
	}

	if (cancelled) { 
		GST_DEBUG("Wait cancelled");
		ret = -1;
	}

	pthread_mutex_unlock (&lock);


	return ret;
}

void	audioSink::signal (bool cancel) {
	pthread_mutex_lock (&lock);

	if (cancel)
		cancelled = true;

	pthread_cond_signal (&sig);

	pthread_mutex_unlock (&lock);
}
