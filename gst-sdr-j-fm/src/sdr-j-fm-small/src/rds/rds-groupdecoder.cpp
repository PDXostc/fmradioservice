#
/*
 *    Copyright (C) 2014
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Programming
 *
 *    This part of the FM demodulation software is largely
 *    a rewrite and local adaptation of FMSTACK software
 *    Technical University of Munich, Institute for Integrated Systems (LIS)
 *    FMSTACK Copyright (C) 2010 Michael Feilen
 * 
 *    Author(s)       : Michael Feilen (michael.feilen@tum.de)
 *    Initial release : 01.09.2009
 *    Last changed    : 09.03.2010
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
 *
 */

#include	"rds-groupdecoder.h"
#include	<cstring>
#include	<gst/gst.h>
#include	<stdio.h>

GST_DEBUG_CATEGORY_EXTERN (sdrjfm_debug);
#define GST_CAT_DEFAULT sdrjfm_debug


rdsGroupDecoder::rdsGroupDecoder (ClearCallback labelClearCallback,
				  StringCallback labelChangeCallback,
				  StringCallback labelCompleteCallback,
				  ClearCallback textClearCallback,
				  StringCallback textChangeCallback,
				  StringCallback textCompleteCallback,
				  void *callbackUserData) {
	this -> labelClearCallback = labelClearCallback;
	this -> labelChangeCallback = labelChangeCallback;
	this -> labelCompleteCallback = labelCompleteCallback;
	this -> textClearCallback = textClearCallback;
	this -> textChangeCallback = textChangeCallback;
	this -> textCompleteCallback = textCompleteCallback;
	this -> callbackUserData = callbackUserData;
	stationLabel[STATION_LABEL_LENGTH] = '\0';
	reset ();
}

	rdsGroupDecoder::~rdsGroupDecoder(void) {
}

void	rdsGroupDecoder::reset (void) {
	m_piCode = 0;

// Initialize Group 1 members
	memset (stationLabel, ' ', STATION_LABEL_LENGTH * sizeof (char));
	stationNameSegmentRegister	= 0;
	m_grp1_diCode		= 0;

// Initialize Group 2 members
	memset (textBuffer, ' ',
	          NUM_OF_CHARS_RADIOTEXT * sizeof (char));
	textABflag		= -1; // Not defined
	textSegmentRegister	= 0;

	if (labelClearCallback)
		labelClearCallback(callbackUserData);
	if (textClearCallback)
		textClearCallback(callbackUserData);
}

bool rdsGroupDecoder::decode (RDSGroup *grp) {
//	fprintf (stderr, "Got group %d\n", grp -> getGroupType ());
	// FIXME: Signals needed
	//setGroup	(grp -> getGroupType ());
	//setPTYCode	(grp -> getProgrammeType ());
	//setPiCode	(grp -> getPiCode ());
	//GST_DEBUG ("RDS Pi code: %x", (unsigned int) grp -> getPiCode ());

//	PI-code has changed -> new station received
//	Reset the decoder
	if (grp -> getPiCode() != m_piCode) {
	   reset ();
	   m_piCode = grp -> getPiCode();
	}

//	Cannot decode B type groups
	if (grp -> isTypeBGroup()) return false;

//	Decide by group type code
	switch (grp -> getGroupType()) {
	   case RDSGroup::BASIC_TUNING_AND_SWITCHING:
	      Handle_Basic_Tuning_and_Switching (grp);
	      break;
//
	   case RDSGroup::RADIO_TEXT:
	      Handle_RadioText (grp);
	      break;

	   case RDSGroup::CLOCKTIME_AND_DATE:
	      Handle_Time_and_Date (grp);
	      break;

//
// Group 8: Open data application. Not implemented yet
//
	   case RDSGroup::TMC_DATA:
	      {  const uint16_t blk_B = grp -> getBlock (RDSGroup::BLOCK_B);
	         const uint16_t blk_C = grp -> getBlock (RDSGroup::BLOCK_C);
	         const uint32_t location = grp -> getBlock(RDSGroup::BLOCK_D);
	         const uint32_t event = blk_C & 0x3FF;
//	         const uint32_t extend = (blk_C >> 11) & 0x7;
//	         const uint32_t direction = (blk_C >> 14) & 0x1;
//	         const uint32_t diversionAdvice = (blk_C >> 15) & 0x1;
//	         const uint32_t duration = (blk_B & 0x7);
	         const uint32_t singleGroupMsg = (blk_B >> 3) & 0x1;
	         const uint32_t tuningInfo = (blk_B >> 4) & 0x1;

	         if (singleGroupMsg == 1 && tuningInfo == 0) {
	            if (location > 51321 || event > 10000) {
		// ERROR!
	            }
	            else {
//	               m_pSink -> addTMCLocationAndEvent (location, event);
	               ;
	            }
	         }
	         break;
	      }
	   default:
	      ;		// just ignore for now
	}

	return true;
}

void	rdsGroupDecoder::Handle_Basic_Tuning_and_Switching (RDSGroup *grp) {
uint32_t segIndex		= grp -> getBlock_B () & 0x3;
uint32_t charsforStationName	= grp -> getBlock_D () & 0xFFFF;

	addtoStationLabel (segIndex, charsforStationName);
	additionalFrequencies (grp -> getBlock_C ());

//	Set Music/Speech flag
	// FIXME: Signals needed
	//setMusicSpeechFlag ((grp -> getBlock_B () >> 3) & 1);

//	Fill DI code
	m_grp1_diCode |= ((grp -> getBlock_B () >> 2) & 1) << segIndex;
}

void	rdsGroupDecoder::addtoStationLabel (uint32_t index,
	                                    uint32_t name) {
	char * segment = &stationLabel [2 * index];

	const char old[2] = { segment [0], segment [1] };

	segment [0] = (char)(name >> 8);
	segment [1] = (char)(name & 0xFF);

	if (labelChangeCallback
	    && (old [0] != segment [0] || old [1] != segment [1]))
		labelChangeCallback(stationLabel, callbackUserData);

//	Reset segment counter on first segment
	if (index == 0)
	   stationNameSegmentRegister = 0;

//	Mark current segment as received, (set bit in segment register to 1)
	//stationNameSegmentRegister |= (2 * index);
	stationNameSegmentRegister |= (1 << index);

//	check whether all segments are in
	if ((int32_t)stationNameSegmentRegister + 1 ==
	                     (1 << NUMBER_OF_NAME_SEGMENTS)) {
	   stationNameSegmentRegister = 0;
	   if (labelCompleteCallback)
		   labelCompleteCallback(stationLabel, callbackUserData);
	}
}

void	rdsGroupDecoder::additionalFrequencies (uint16_t blockContents) {
uint8_t af1 = blockContents >> 8;
uint8_t af2 = blockContents & 0xFF;

	if ((af1 > 1) && (af1 < 205)) {
	   // FIXME: Signals needed
	   //setAFDisplay (af1 * 100 + 87500);
	}

//	Check for range and add only VHF frequencies
	if ((af1 != 250) && (af2 > 1) && (af1 < 205))  {
	   // FIXME: Signals needed
	   //setAFDisplay (af2 * 100 + 87500);
	}
}

void	rdsGroupDecoder::Handle_RadioText (RDSGroup *grp) {
const uint16_t new_txtABflag = (grp -> getBlock_B () >> 4) & 1;
const uint16_t currentSegment = (grp -> getBlock_B ()) & 0xF;
char* textFragment = &textBuffer [4 * currentSegment];
bool	endF	= false;
uint16_t	textPart1, textPart2;
uint16_t	i;

	if (textABflag != new_txtABflag) {
	   textABflag = new_txtABflag;

//	If the textA/B has changed, we clear the old displayed message ...
	   textSegmentRegister = 0;
	   memset (textBuffer, ' ',
	          NUM_OF_CHARS_RADIOTEXT * sizeof (char));

	   if (textClearCallback)
		   textClearCallback (callbackUserData);
	}

	textPart1	= grp -> getBlock_C ();
	textPart2	= grp -> getBlock_D ();

 	// Store the received data
	textFragment [0] = (char)(textPart1 >> 8);
	textFragment [1] = (char)(textPart1 & 0xFF);
	textFragment [2] = (char)(textPart2 >> 8);
	textFragment [3] = (char)(textPart2 & 0xFF);

//	current segment is received (set bit in segment register to 1)
	textSegmentRegister |= 1 << currentSegment;
	if (textChangeCallback)
	    textChangeCallback (textBuffer, callbackUserData);

//	check for end of message
	for (i = 0; i < 4; i ++)
	    if (textFragment [i] == END_OF_RADIO_TEXT) {
	        endF = true;
		break;
	    }

// Check if all fragments are in or we had an end of message
	if (endF ||
	    (textSegmentRegister == (1 << NUM_OF_FRAGMENTS) - 1)) {
	     if (textCompleteCallback)
		 textCompleteCallback (textBuffer, callbackUserData);
	     textSegmentRegister = 0;
	     memset (textBuffer, ' ',
	          NUM_OF_CHARS_RADIOTEXT * sizeof (char));
	}
}

void	rdsGroupDecoder::Handle_Time_and_Date (RDSGroup *grp) {
uint16_t Hours	= (grp -> getBlock_D () >> 12) & 0xF;
uint16_t Minutes = (grp -> getBlock_D () >> 6) & 0x3F;
uint16_t Days    = grp -> getBlock_C ();
uint16_t offset  = (grp -> getBlock_D ()) & 0x4F;

	fprintf (stderr, "Time = %d:%d (Days = %d) \n",
	                   Hours + offset / 2, Minutes, Days);
}

