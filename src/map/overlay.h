/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 * Author: NikitaFeodonit, nfeodonit@yandex.com
 ******************************************************************************
 *   Copyright (c) 2016-2017 NextGIS, <info@nextgis.com>
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

#ifndef NGSOVERLAY_H
#define NGSOVERLAY_H

// stl
#include <memory>

#include "ngstore/codes.h"

namespace ngs
{

class Overlay
{
public:
    explicit Overlay(ngsMapOverlyType type = MOT_UNKNOWN);
    virtual ~Overlay() = default;

    ngsMapOverlyType type() const { return m_type; }
    bool visible() const { return m_visible; }
    void setVisible(bool visible) { m_visible = visible; }

    static int getOverlayIndexFromType(ngsMapOverlyType type)
    {
        // Overlays stored in reverse order
        switch (type) {
            case MOT_EDIT:
                return 0;
            case MOT_TRACK:
                return 1;
            case MOT_LOCATION:
                return 2;
            default:
                return -1;
        }
    }

protected:
    enum ngsMapOverlyType m_type;
    bool m_visible;
};

typedef std::shared_ptr<Overlay> OverlayPtr;

class CurrentLocationOverlay : public Overlay
{
public:
    explicit CurrentLocationOverlay();
    virtual ~CurrentLocationOverlay() = default;
};

class CurrentTrackOverlay : public Overlay
{
public:
    explicit CurrentTrackOverlay();
    virtual ~CurrentTrackOverlay() = default;
};

class EditLayerOverlay : public Overlay
{
public:
    explicit EditLayerOverlay();
    virtual ~EditLayerOverlay() = default;
};

}  // namespace ngs

#endif  // NGSOVERLAY_H
