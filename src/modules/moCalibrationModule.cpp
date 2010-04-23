/***********************************************************************
 ** Copyright (C) 2010 Movid Authors.  All rights reserved.
 **
 ** This file is part of the Movid Software.
 **
 ** This file may be distributed under the terms of the Q Public License
 ** as defined by Trolltech AS of Norway and appearing in the file
 ** LICENSE included in the packaging of this file.
 **
 ** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 ** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 **
 ** Contact info@movid.org if any conditions of this licensing are
 ** not clear to you.
 **
 **********************************************************************/


#include <assert.h>
#include "sstream"
#include "moCalibrationModule.h"
#include "../moLog.h"
#include <stdio.h>

// XXX Desc
MODULE_DECLARE(Calibration, "native", "Calibration");

// callback used for rows and cols. if these properties is updated,
// refresh the GUI.
void mocalibrationmodule_update_size(moProperty *property, void *userdata)
{
	moCalibrationModule *module = static_cast<moCalibrationModule *>(userdata);
	assert(userdata != NULL);
	module->buildScreenPoints();
}

void mocalibrationmodule_activate_calibration(moProperty *property, void *userdata)
{
	moCalibrationModule *module = static_cast<moCalibrationModule *>(userdata);
	assert(userdata != NULL);
	if ( property->asBool() )
		module->resetCalibration();
}

moCalibrationModule::moCalibrationModule() : moModule(MO_MODULE_INPUT | MO_MODULE_OUTPUT | MO_MODULE_GUI, 1, 1){

	MODULE_INIT();

	// declare input/output
	this->input_infos[0] = new moDataStreamInfo("data", "moDataGenericList", "Data stream with type of 'touch' or 'fiducial'");
	this->output_infos[0] = new moDataStreamInfo("data", "moDataGenericList", "Data stream with type of 'touch' or 'fiducial'");

	this->properties["rows"] = new moProperty(3);
	this->properties["rows"]->setMin(2);
	this->properties["rows"]->addCallback(mocalibrationmodule_update_size, this);
	this->properties["cols"] = new moProperty(3);
	this->properties["cols"]->setMin(2);
	this->properties["cols"]->addCallback(mocalibrationmodule_update_size, this);
	this->properties["screenPoints"] = new moProperty(moPointList());
	this->properties["calibrate"] = new moProperty(true);
	this->properties["calibrate"]->addCallback(mocalibrationmodule_activate_calibration, this);

	// XXX
	this->retriangulate = true;
	this->rect = cvRect(0, 0, 5000, 5000);
	this->storage = cvCreateMemStorage(0);
	this->active_point = 0;
	this->last_id = 0;
	this->input = NULL;
	this->output = NULL;
	this->subdiv = NULL;

	this->buildScreenPoints();
}

void moCalibrationModule::resetCalibration() {
	this->last_id = 0;
	this->active_point = 0;
}

void moCalibrationModule::buildScreenPoints() {
	// TODO handle saving of additionnal point in another list than screenPoints
	int mx = this->property("cols").asInteger();
	int my = this->property("rows").asInteger();
	float dx = 1.0 / ((float)mx - 1);
	float dy = 1.0 / ((float)my - 1);
	std::ostringstream oss;
	oss.str("");
	for ( int x = 0; x < mx; x++ ) {
		for ( int y = 0; y < my; y++ ) {
			oss << (x * dx) << ",";
			oss << (y * dy) << ";";
		}
	}

	this->property("screenPoints").set(oss.str());
	this->notifyGui();
}

moCalibrationModule::~moCalibrationModule() {
}

void moCalibrationModule::guiFeedback(const std::string& type, double x, double y) {
	this->notifyGui();
}

void moCalibrationModule::guiBuild(void) {
	std::ostringstream oss;
	moPointList screenPoints = this->property("screenPoints").asPointList();
	moPointList::iterator it;
	unsigned int index = 0;

	this->gui.clear();
	this->gui.push_back("viewport 1000 1000");
	this->gui.push_back("color 0 121 184");

	for ( it = screenPoints.begin(); it != screenPoints.end(); it++ ) {
		if ( index == this->active_point )
			this->gui.push_back("color 255 255 255");

		oss.str("");
		oss << "circle " << (int)(it->x * 1000.) << " " << (int)(it->y * 1000.) << " 50";
		this->gui.push_back(oss.str());

		if ( index == this->active_point )
			this->gui.push_back("color 120 120 120");

		index++;
	}
}

CvSubdiv2D* init_delaunay(CvMemStorage* storage, CvRect rect) {
    CvSubdiv2D* subdiv;
    subdiv = cvCreateSubdiv2D(CV_SEQ_KIND_SUBDIV2D,
							  sizeof(*subdiv),
							  sizeof(CvSubdiv2DPoint),
							  sizeof(CvQuadEdge2D),
							  storage);
    cvInitSubdivDelaunay2D(subdiv, rect);
    return subdiv;
}

void moCalibrationModule::triangulate() {/*
	std::cout << "Points:" << std::endl;
	moPointList points = this->property("grid_points").asPointList();
	moPointList::iterator it;
	this->subdiv = init_delaunay(this->storage, this->rect);
	for(it = points.begin(); it != points.end() ; it++) {
		std::cout << (*it).x << " " << (*it).y << std::endl;
		// XXX crashes here. cvPoint2D32f is a convenience macro for double to cvp2d32f conversion
		CvPoint2D32f fp = cvPoint2D32f((*it).x, (*it).y);
		//{static_cast<float>((*it).x),
		//static_cast<float>((*it).y)};
		cvSubdivDelaunay2DInsert(subdiv, fp);
	}
	// XXX Is triangulation performed now already?

	// Take the result of the triangulation and put them into our triangle struct
	// TODO
	this->property("needs_retriangulation") = false;
	*/
}

void moCalibrationModule::calibrate() {
	moDataGenericList *blobs = static_cast<moDataGenericList*>(input->getData());
	moDataGenericContainer *touch;
	moPointList screenPoints;
	moPoint surfacePoint, p;

	LOG(MO_DEBUG) << "#Blobs in frame: " << blobs->size();

	// We only calibrate the current point if there is an unambiguous amount of touches
	if (blobs->size() != 1)
		return;

	// We now want to assign each point its coordinates on the touch surface
	touch = (*blobs)[0];

	// Don't reuse the same touch id as before
	if ( touch->properties["id"]->asInteger() == this->last_id )
		return;
	this->last_id = touch->properties["id"]->asInteger();

	// TODO While calibrating, this list MUST NOT change!
	screenPoints = this->property("screenPoints").asPointList();
	if (this->active_point == screenPoints.size()) {
		// We have calibrated all points, so we're done.
		LOG(MO_DEBUG) << "Calibration complete!";
		this->active_point = 0;
		this->property("calibrate").set(false);
		return;
	}


	LOG(MO_DEBUG) << "# of screenPoints: " << screenPoints.size();
	LOG(MO_DEBUG) << "Processing point #" << this->active_point;

	// We're starting calibration again, so discard all old calibration results
	if (this->active_point == 0)
		this->surfacePoints.clear();

	// screenPoints and corresponding surfacePoint have the same index in their respective vector
	surfacePoint.x = touch->properties["x"]->asDouble();
	surfacePoint.y = touch->properties["y"]->asDouble();
	this->surfacePoints.push_back(surfacePoint);
	
	p = screenPoints[this->active_point];
	LOG(MO_DEBUG) << "(" << p.x << ", " << p.y << ") is mapped to (" \
		<< surfacePoint.x << ", " << surfacePoint.y << ").";

	// Proceed with the next grid point
	this->active_point++;

	// Perhaps points were added, moved or deleted. If this is the case
	// we have to triangulate again. 
	if (this->retriangulate)
		this->triangulate();

	// Inform gui to update
	this->notifyGui();
}

void moCalibrationModule::update() {
	bool calibrate = this->property("calibrate").asBool();

	assert(this->input != NULL);

	this->input->lock();	
	
	if ( calibrate ) {
		this->calibrate();
	} else {
		// Calibration & triangulation is done. Just convert the point coordinates.
	}	
	
	this->input->unlock();	
}

void moCalibrationModule::notifyData(moDataStream *input) {
	assert( input != NULL );
	assert( input == this->input );
	this->notifyUpdate();
}

void moCalibrationModule::start() {
	moModule::start();
}

void moCalibrationModule::stop() {
	moModule::stop();
}

void moCalibrationModule::setInput(moDataStream *stream, int n) {
	if ( n != 0 ) {
		this->setError("Invalid input index");
		return;
	}
	if ( this->input != NULL )
		this->input->removeObserver(this);
	this->input = stream;
	if ( stream != NULL ) {
		if ( stream->getFormat() != "GenericTouch" &&
			stream->getFormat() != "GenericFiducial" ) {
			this->setError("Input 0 accept only touch or fiducial");
			this->input = NULL;
			return;
		}
	}
	if ( this->input != NULL )
		this->input->addObserver(this);
}

moDataStream* moCalibrationModule::getInput(int n) {
	if ( n != 0 ) {
		this->setError("Invalid input index");
		return NULL;
	}
	return this->input;
}

moDataStream* moCalibrationModule::getOutput(int n) {
	return this->output;
}