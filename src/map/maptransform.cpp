/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016 NextGIS, <info@nextgis.com>
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/
#include "maptransform.h"
#include "constants.h"
#include "geometryutil.h"

#include <algorithm>
#include <iostream>

using namespace std;
using namespace ngs;

MapTransform::MapTransform(int width, int height) : m_displayWidht(width),
    m_displayHeight(height), m_sizeChanged(true), m_rotate(0),
    m_ratio(DEFAULT_RATIO)
{
    setExtent(setEnvelope(DEFAULT_MIN_X, DEFAULT_MAX_X, DEFAULT_MIN_Y,
                          DEFAULT_MAX_Y));
    setDisplaySize(width, height);
}

MapTransform::~MapTransform()
{

}

int MapTransform::getDisplayHeight() const
{
    return m_displayHeight;
}
int MapTransform::getDisplayWidht() const
{
    return m_displayWidht;
}

bool MapTransform::isSizeChanged() const
{
    return m_sizeChanged;
}

void MapTransform::setSizeChanged(bool sizeChanged)
{
    m_sizeChanged = sizeChanged;
}

double MapTransform::getRotate() const
{
    return m_rotate;
}

void MapTransform::setRotate(double rotate)
{
    m_rotate = rotate;
}

OGREnvelope MapTransform::getExtent() const
{
    return m_extent;
}

OGRRawPoint MapTransform::getCenter() const
{
    return m_center;
}

OGRRawPoint MapTransform::worldToDisplay(const OGRRawPoint &pt)
{
    return m_worldToDisplayMatrix.project (pt);
}

OGRRawPoint MapTransform::displayToWorld(const OGRRawPoint &pt)
{
    return m_invWorldToDisplayMatrix.project (pt);
}

void MapTransform::setDisplaySize(int width, int height)
{
    m_sizeChanged = true;
    m_displayWidht = width;
    m_displayHeight = height;

    double scaleX = fabs(double(m_displayWidht)) * .5;
    double scaleY = fabs(double(m_displayHeight)) * .5;

    m_scaleView = min(scaleX, scaleY);

    m_ratio = double(width) / height;

    updateExtent();
}

bool MapTransform::setScale(double scale)
{
    m_scale = scale;
    return updateExtent();
}

bool MapTransform::setCenter(double x, double y)
{
    m_center.x = x;
    m_center.y = y;
    return updateExtent();
}

bool MapTransform::setScaleAndCenter(double scale, double x, double y)
{
    m_scale = scale;
    m_center.x = x;
    m_center.y = y;
    return updateExtent();
}

bool MapTransform::setExtent(const OGREnvelope &env)
{
    m_center = getEnvelopeCenter (env);
    m_extent = setEnvelopeRatio(env, m_ratio);
    double w = getEnvelopeWidth (env);
    double h = getEnvelopeHeight (env);
    double scaleX = fabs(double(m_displayWidht) / w);
    double scaleY = fabs(double(m_displayHeight) / h);
    m_scale = min(scaleX, scaleY);

    scaleX = 1.0 / w;
    scaleY = 1.0 / h;
    m_scaleScene = min(scaleX, scaleY);

    if(!isEqual(m_rotate, 0.0)){
        m_extent = rotateEnvelope (m_extent, m_rotate);
    }

    initMatrices();
    return true; // return false if extent modified to fit limits
}

bool MapTransform::updateExtent()
{
    double doubleScale = m_scale * 2;
    double halfWidth = double(m_displayWidht) / doubleScale;
    double halfHeight = double(m_displayHeight) / doubleScale;
    m_extent.MinX = m_center.x - halfWidth;
    m_extent.MaxX = m_center.x + halfWidth;
    m_extent.MinY = m_center.y - halfHeight;
    m_extent.MaxY = m_center.y + halfHeight;

    double scaleX = 1.0 / (halfWidth + halfWidth);
    double scaleY = 1.0 / (halfHeight + halfHeight);
    m_scaleScene = min(scaleX, scaleY);

    if(!isEqual(m_rotate, 0.0)){
        m_extent = rotateEnvelope (m_extent, m_rotate);
    }

    initMatrices();
    return true;  // return false if extent modified to fit limits
}

void MapTransform::initMatrices()
{
    // world -> scene matrix
    m_sceneMatrix.clear ();
    m_sceneMatrix.ortho (m_extent.MinX, m_extent.MaxX,
                         m_extent.MaxY, m_extent.MinY, -1, 1);

    if(!isEqual(m_rotate, 0.0)){
        // TODO: rotate m_sceneMatrix
    }

    // world -> scene inv matrix
    m_invSceneMatrix = m_sceneMatrix;
    m_invSceneMatrix.invert ();

    // scene -> view inv matrix
    m_invViewMatrix.clear ();
    m_invViewMatrix.ortho (0, m_displayWidht, m_displayHeight, 0, -1, 1);

    // scene -> view matrix
    m_viewMatrix = m_invViewMatrix;
    m_viewMatrix.invert ();

    m_worldToDisplayMatrix = m_viewMatrix;
    m_worldToDisplayMatrix.multiply (m_sceneMatrix);

    m_invWorldToDisplayMatrix = m_invViewMatrix;
    m_invWorldToDisplayMatrix.multiply (m_invSceneMatrix);
}

double MapTransform::getScale() const
{
    return m_scale;
}

Matrix4 MapTransform::getSceneMatrix() const
{
    return m_sceneMatrix;
}

double MapTransform::getZoom() const {
    double retVal = log(m_scale) / M_LN2;
    return retVal < 0 ? 0 : retVal;
}
