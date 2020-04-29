﻿/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2020 NextGIS, <info@nextgis.com>
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
#ifndef NGSGEOMETRY_H
#define NGSGEOMETRY_H

// gdal
#include "cpl_json.h"
#include "ogrsf_frmts.h"
#include "ogr_geometry.h"

// std
#include <array>
#include <memory>
#include <set>

#include "api_priv.h"
#include "ngstore/util/constants.h"
#include "util/buffer.h"
#include "coordinatetransformation.h"

namespace ngs {

constexpr double BIG_VALUE = 100000000.0; // 100 000 000
constexpr float BIG_VALUE_F = 100000000.0f; // 100 000 000

class GeometryPtr : public std::shared_ptr<OGRGeometry>
{
public:
    GeometryPtr(OGRGeometry *geom);
    GeometryPtr();
    GeometryPtr &operator=(OGRGeometry *geom);
    operator OGRGeometry*() const;
};

class Envelope
{
public:
    Envelope();
    constexpr Envelope(double minX, double minY, double maxX, double maxY) :
        m_minX(minX),
        m_minY(minY),
        m_maxX(maxX),
        m_maxY(maxY)
    { }
    Envelope(const OGREnvelope &env);
    void set(const OGREnvelope &env);

    void operator=(const OGREnvelope &env);

    bool isInit() const;
    void clear();
    OGRRawPoint center() const;
    void rotate(double angle);
    void setRatio(double ratio);
    void resize(double value);
    void move(double deltaX, double deltaY);
    constexpr double width() const { return m_maxX - m_minX; }
    constexpr double height() const { return m_maxY - m_minY; }
    GeometryPtr toGeometry(SpatialReferencePtr spatialRef) const;
    OGREnvelope toOgrEnvelope() const;

    constexpr double minX() const { return m_minX; }
    constexpr double minY() const { return m_minY; }
    constexpr double maxX() const { return m_maxX; }
    constexpr double maxY() const { return m_maxY; }

    void setMinX(double minX) { m_minX = minX; }
    void setMinY(double minY) { m_minY = minY; }
    void setMaxX(double maxX) { m_maxX = maxX; }
    void setMaxY(double maxY) { m_maxY = maxY; }

    bool load(const CPLJSONObject &store, const Envelope &defaultValue);
    CPLJSONObject save() const;
    bool intersects(const Envelope &other) const;
    bool contains(Envelope const &other) const;
    const Envelope &merge( Envelope const &other );
    const Envelope &intersect( Envelope const &other );
    void fix();

protected:
    double m_minX, m_minY, m_maxX, m_maxY;
};

constexpr unsigned short DEFAULT_EPSG = 3857;

constexpr Envelope DEFAULT_BOUNDS = Envelope(-20037508.34, -20037508.34,
                                       20037508.34, 20037508.34);
constexpr Envelope DEFAULT_BOUNDS_X2 = Envelope(DEFAULT_BOUNDS.minX() * 2,
                                          DEFAULT_BOUNDS.minY() * 2,
                                          DEFAULT_BOUNDS.maxX() * 2,
                                          DEFAULT_BOUNDS.maxY() * 2);
constexpr Envelope DEFAULT_BOUNDS_Y2X4 = Envelope(DEFAULT_BOUNDS.minX() * 4,
                                          DEFAULT_BOUNDS.minY() * 2,
                                          DEFAULT_BOUNDS.maxX() * 4,
                                          DEFAULT_BOUNDS.maxY() * 2);

//    OGRGeometry* simplifyGeometry(const OGRGeometry* geometry, double distance);

typedef struct _normal {
    float x, y;
    bool operator==(const _normal &other) const { return isEqual(x, other.x) &&
                isEqual(y,other.y);}
} Normal, SimplePoint;

Normal ngsGetNormals(const SimplePoint &beg, const SimplePoint &end);

typedef struct _tile{
    int x, y;
    unsigned char z;
    char crossExtent;
    bool operator==(const struct _tile &other) const {
            return x == other.x && y == other.y && z == other.z &&
                    crossExtent == other.crossExtent;
    }
    bool operator<(const struct _tile &other) const {
        return x < other.x || (x == other.x && y < other.y) ||
                (x == other.x && y == other.y && z < other.z) ||
                (x == other.x && y == other.y && z == other.z &&
                 crossExtent < other.crossExtent);
    }
} Tile;

typedef struct _tileItem{
    Tile tile;
    Envelope env;
} TileItem;

OGRGeometry *ngsCreateGeometryFromGeoJson(const CPLJSONObject &json);

bool ngsIsGeometryIntersectsEnvelope(const OGRGeometry &geometry,
                                     const Envelope &env);
double ngsDistance(const OGRRawPoint &pt1, const OGRRawPoint &pt2);
bool ngsIsNear(const OGRRawPoint &pt1, const OGRRawPoint &pt2, double tolerance);
OGRRawPoint ngsGetMiddlePoint(const OGRRawPoint &pt1, const OGRRawPoint &pt2);

class VectorTileItem
{
    friend class VectorTile;
public:
    VectorTileItem();
    void addId(GIntBig id) { m_ids.insert(id); }
    void removeId(GIntBig id);
    void addPoint(const SimplePoint &pt) { m_points.push_back(pt); }
    void addIndex(unsigned short index) { m_indices.push_back(index); }
    void addBorderIndex(unsigned short ring, unsigned short index);
    void addCentroid(const SimplePoint &pt) { m_centroids.push_back(pt); }

    size_t pointCount() const { return m_points.size(); }
    const SimplePoint &point(size_t index) const { return m_points[index]; }
    bool isClosed() const;
    const std::vector<SimplePoint> &points() const { return m_points; }
    const std::vector<unsigned short> &indices() const { return m_indices; }
    const std::vector<std::vector<unsigned short>> &borderIndices() const {
        return m_borderIndices;
    }
    bool isValid() const { return m_valid; }
    void setValid(bool valid) { m_valid = valid; }
    bool operator==(const VectorTileItem &other) const {
        return m_points == other.m_points;
    }
    bool isIdsPresent(const std::set<GIntBig> &other, bool full = true) const;
    std::set<GIntBig> idsIntesect(const std::set<GIntBig> &other) const;

protected:
    void loadIds(const VectorTileItem &item);
    void save(Buffer *buffer);
    bool load(Buffer &buffer);
private:
    std::vector<SimplePoint> m_points;
    std::vector<unsigned short> m_indices;
    std::vector<std::vector<unsigned short>> m_borderIndices; // NOTE: first array is exterior ring indices
    std::vector<SimplePoint> m_centroids;
    std::set<GIntBig> m_ids;
    bool m_valid;
    bool m_2d;
};

using VectorTileItemArray = std::vector<VectorTileItem>;

class VectorTile
{
public:
    VectorTile() : m_valid(false) {}
    void add(const VectorTileItem &item, bool checkDuplicates = false);
    void add(const VectorTileItemArray &items, bool checkDuplicates = false);
    void remove(GIntBig id);
    BufferPtr save();
    bool load(Buffer &buffer);
    VectorTileItemArray items() const { return m_items; }
    bool empty() const;
    bool isValid() const { return m_valid; }
private:
    VectorTileItemArray m_items;
    bool m_valid;
};

class GEOSContextHandlePtr : public std::shared_ptr<struct GEOSContextHandle_HS>
{
public:
    GEOSContextHandlePtr() : shared_ptr(OGRGeometry::createGEOSContext(),
                                        OGRGeometry::freeGEOSContext) {}
};

class GEOSGeometryWrap;
using  GEOSGeometryPtr = std::shared_ptr<GEOSGeometryWrap>;
class GEOSGeometryWrap
{
public:
    explicit GEOSGeometryWrap(GEOSGeom geom, GEOSContextHandlePtr handle);
    explicit GEOSGeometryWrap(OGRGeometry *geom);
    ~GEOSGeometryWrap();
    GEOSGeom geom() const { return m_geom; }
    int type() const;
    GEOSGeometryPtr clip(const Envelope &env) const;
    void simplify(double step);
    bool isValid() const { return m_geom != nullptr; }
    void fillTile(GIntBig fid, VectorTileItemArray &vitemArray);
    double distance(double x, double y) const;
    bool intersects(double x, double y) const;

private:
    GEOSGeom generalizePoint(const GEOSGeom_t *geom, double step);
    GEOSGeom generalizeMultiPoint(const GEOSGeom_t *geom, double step);
    GEOSGeom generalizeLine(const GEOSGeom_t *geom, double step, bool isRing = false);
    GEOSGeom generalizeMultiLine(const GEOSGeom_t *geom, double step);
    GEOSGeom generalizePolygon(const GEOSGeom_t *geom, double step);
    GEOSGeom generalizeMultiPolygon(const GEOSGeom_t *geom, double step);
    void setCentroid(int type);
    void fillPointTile(GIntBig fid, const GEOSGeom_t *geom,
                       VectorTileItemArray& vitemArray);
    void fillMultiPointTile(GIntBig fid, const GEOSGeom_t *geom,
                            VectorTileItemArray &vitemArray);
    void fillLineTile(GIntBig fid, const GEOSGeom_t *geom,
                       VectorTileItemArray &vitemArray);
    void fillMultiLineTile(GIntBig fid, const GEOSGeom_t *geom,
                       VectorTileItemArray &vitemArray);
    void fillPolygonTile(GIntBig fid, const GEOSGeom_t *geom,
                       VectorTileItemArray &vitemArray);
    void fillMultiPolygonTile(GIntBig fid, const GEOSGeom_t *geom,
                       VectorTileItemArray &vitemArray);
    void fillCollectionTile(GIntBig fid, const GEOSGeom_t *geom,
                       VectorTileItemArray &vitemArray);

private:
    GEOSGeom m_geom;
    GEOSContextHandlePtr m_geosHandle;
};

/**
 * @brief The EditGeometryData class.
 */
template<class T> class EditGeometryData
{
public:
    EditGeometryData();
    ~EditGeometryData() = default;
    bool canUndo() const;
    bool canRedo() const;
    bool undo();
    bool redo();
    void saveState();

    T m_data;
    std::vector<T> m_history;
    
protected:
    size_t m_currentEditStep;
};

/**
 * @brief The EditGeometry class. Base class for editing geometries
 */
class EditGeometry
{
public:
    enum class PieceType {
        POINT = 1,
        HOLE,
        PART,
    };
    enum class Type {
        POINT = 1,
        LINE,
        POLYGON,
        MULTIPOINT,
        MULTILINE,
        MULTIPOLYGON,
    };

public:
    EditGeometry();
    virtual ~EditGeometry();
    virtual bool canUndo() const = 0;
    virtual bool canRedo() const = 0;
    virtual bool undo() = 0;
    virtual bool redo() = 0;
    virtual OGRGeometry *toGDALGeometry() const = 0;
    virtual bool isValid() const;
    virtual ngsPointId touch(const OGRRawPoint &pt, enum ngsMapTouchType type,
                             double tolerance);
    virtual bool addPoint(double x, double y, bool log = true);
    virtual bool addPiece(enum PieceType type, double x1, double y1,
                          double x2, double y2);
    virtual enum ngsEditDeleteResult deletePiece(enum PieceType type);
    virtual enum Type type() const = 0;
    int selectedPoint() const { return  m_selectedPoint.pointId; }

    // static
public:
    static EditGeometry *fromGDALGeometry(OGRGeometry *geom);

protected:
    virtual ngsPointId selectNearestPoint(const OGRRawPoint &pt, double tolerance);
    virtual bool isNearestSelected(const OGRRawPoint &pt, double tolerance) = 0;
    virtual void updateSelectedPoint(const OGRRawPoint &pt, bool log = false) = 0;
    virtual GEOSGeom toGEOSGeometry(GEOSContextHandlePtr handle) const = 0;

protected:
    ngsPointId m_selectedPoint;
    bool m_isDragging;    
};

using EditGeometryUPtr = std::unique_ptr<EditGeometry>;

/**
 * @brief The EditPoint class
 */
class EditPoint : public EditGeometry
{
public:
    explicit EditPoint(double x, double y);
    explicit EditPoint(OGRPoint *pt);
    OGRRawPoint data() const { return m_data.m_data; }

protected:
    EditGeometryData<OGRRawPoint> m_data;

    // EditGeometry interface
public:
    virtual enum Type type() const override { return Type::POINT; }
    virtual bool canUndo() const override;
    virtual bool canRedo() const override;
    virtual bool undo() override;
    virtual bool redo() override;
    virtual OGRGeometry *toGDALGeometry() const override;

protected:
    virtual bool isNearestSelected(const OGRRawPoint &pt,
                                   double tolerance) override;
    virtual void updateSelectedPoint(const OGRRawPoint &pt,
                                     bool log = false) override;
    virtual ngsPointId selectNearestPoint(const OGRRawPoint &pt,
                                          double tolerance) override;
    virtual GEOSGeom toGEOSGeometry(GEOSContextHandlePtr handle) const override;
};

/**
 * @brief The EditLine class
 */
using Line = std::vector<OGRRawPoint>;
class EditLine : public EditGeometry
{
public:
    EditLine();
    explicit EditLine(OGRLineString *line);
    void init(double x1, double y1, double x2, double y2);
    std::vector<OGRRawPoint> data() const { return m_data.m_data; }
    int selectedPart() const { return m_selectedPoint.pointId == NOT_FOUND ? NOT_FOUND : 0; }

protected:
    EditGeometryData<Line> m_data;

    // EditGeometry interface
public:
    virtual enum Type type() const override { return Type::LINE; }
    virtual bool canUndo() const override;
    virtual bool canRedo() const override;
    virtual bool undo() override;
    virtual bool redo() override;
    virtual OGRGeometry *toGDALGeometry() const override;
    virtual bool isValid() const override;
    virtual bool addPoint(double x, double y, bool log = true) override;
    virtual enum ngsEditDeleteResult deletePiece(enum PieceType type) override;

protected:
    virtual bool isNearestSelected(const OGRRawPoint &pt,
                                   double tolerance) override;
    virtual void updateSelectedPoint(const OGRRawPoint &pt,
                                     bool log = false) override;
    virtual ngsPointId selectNearestPoint(const OGRRawPoint &pt,
                                          double tolerance) override;
    virtual GEOSGeom toGEOSGeometry(GEOSContextHandlePtr handle) const override;

protected:
    void insertPoint(int index, const OGRRawPoint &pt);
};

/**
 * @brief The EditPolygon class
 */
using Polygon = std::vector<Line>;
class EditPolygon : public EditGeometry
{
public:
    EditPolygon();
    explicit EditPolygon(OGRPolygon *poly);
    void init(double x1, double y1, double x2, double y2);    
    std::vector<Line> data() const { return m_data.m_data; }
    int selectedRing() const { return m_selectedRing; }
    int selectedPart() const { return m_selectedRing == NOT_FOUND ? NOT_FOUND : 0; }
    
protected:
    EditGeometryData<Polygon> m_data;
    int m_selectedRing;
    
    // EditGeometry interface
public:
    virtual enum Type type() const override { return Type::POLYGON; }
    virtual bool canUndo() const override;
    virtual bool canRedo() const override;
    virtual bool undo() override;
    virtual bool redo() override;
    virtual OGRGeometry *toGDALGeometry() const override;
    virtual bool isValid() const override;
    virtual bool addPoint(double x, double y, bool log = true) override;
    virtual bool addPiece(enum PieceType type, double x1, double y1,
                          double x2, double y2) override;
    virtual enum ngsEditDeleteResult deletePiece(enum PieceType type) override;

protected:
    virtual bool isNearestSelected(const OGRRawPoint &pt,
                                   double tolerance) override;
    virtual void updateSelectedPoint(const OGRRawPoint &pt,
                                     bool log = false) override;
    virtual ngsPointId selectNearestPoint(const OGRRawPoint &pt,
                                          double tolerance) override;
    virtual GEOSGeom toGEOSGeometry(GEOSContextHandlePtr handle) const override;

protected:
    void insertPoint(int index, const OGRRawPoint &pt);
};

/**
 * @brief The EditMultiPoint class
 */
class EditMultiPoint : public EditGeometry
{
public:
    EditMultiPoint();
    explicit EditMultiPoint(OGRMultiPoint *mpoint);
    void init(double x, double y);
    std::vector<OGRRawPoint> data() const { return m_data.m_data; }

protected:
    using MultiPoint = std::vector<OGRRawPoint>;
    EditGeometryData<MultiPoint> m_data;
    int m_selectedPart;

    // EditGeometry interface
public:
    virtual enum Type type() const override { return Type::MULTIPOINT; }
    virtual bool canUndo() const override;
    virtual bool canRedo() const override;
    virtual bool undo() override;
    virtual bool redo() override;
    virtual OGRGeometry *toGDALGeometry() const override;
    virtual bool isValid() const override;
    virtual bool addPoint(double x, double y, bool log = true) override;
    virtual enum ngsEditDeleteResult deletePiece(enum PieceType type) override;

protected:
    virtual bool isNearestSelected(const OGRRawPoint &pt,
                                   double tolerance) override;
    virtual void updateSelectedPoint(const OGRRawPoint &pt,
                                     bool log = false) override;
    virtual ngsPointId selectNearestPoint(const OGRRawPoint &pt,
                                          double tolerance) override;
    virtual GEOSGeom toGEOSGeometry(GEOSContextHandlePtr handle) const override;
};

/**
 * @brief The EditMultiLine class
 */
class EditMultiLine : public EditGeometry
{
public:
    EditMultiLine();
    explicit EditMultiLine(OGRMultiLineString *mline);
    void init(double x1, double y1, double x2, double y2);
    std::vector<Line> data() const { return m_data.m_data; }
    int selectedPart() const { return m_selectedPart; }

protected:
    using MultiLine = std::vector<Line>;
    EditGeometryData<MultiLine> m_data;
    int m_selectedPart;

    // EditGeometry interface
public:
    virtual enum Type type() const override { return Type::MULTILINE; }
    virtual bool canUndo() const override;
    virtual bool canRedo() const override;
    virtual bool undo() override;
    virtual bool redo() override;
    virtual OGRGeometry *toGDALGeometry() const override;
    virtual bool isValid() const override;
    virtual bool addPoint(double x, double y, bool log = true) override;
    virtual bool addPiece(enum PieceType type, double x1, double y1,
                          double x2, double y2) override;
    virtual enum ngsEditDeleteResult deletePiece(enum PieceType type) override;

protected:
    virtual bool isNearestSelected(const OGRRawPoint &pt,
                                   double tolerance) override;
    virtual void updateSelectedPoint(const OGRRawPoint &pt,
                                     bool log = false) override;
    virtual ngsPointId selectNearestPoint(const OGRRawPoint &pt,
                                          double tolerance) override;
    virtual GEOSGeom toGEOSGeometry(GEOSContextHandlePtr handle) const override;

protected:
    void insertPoint(int index, const OGRRawPoint &pt);

};

class EditMultiPolygon : public EditGeometry
{
public:
    EditMultiPolygon();
    explicit EditMultiPolygon(OGRMultiPolygon *mpoly);
    void init(double x1, double y1, double x2, double y2);  
    std::vector<Polygon> data() const { return m_data.m_data; }
    int selectedRing() const { return m_selectedRing; }
    int selectedPart() const { return m_selectedPart; }

protected:
    using MultiPolygon = std::vector<Polygon>;
    EditGeometryData<MultiPolygon> m_data;
    int m_selectedPart;
    int m_selectedRing;

    // EditGeometry interface
public:
    virtual enum Type type() const override { return Type::MULTIPOLYGON; }
    virtual bool canUndo() const override;
    virtual bool canRedo() const override;
    virtual bool undo() override;
    virtual bool redo() override;
    virtual OGRGeometry *toGDALGeometry() const override;
    virtual bool isValid() const override;
    virtual bool addPoint(double x, double y, bool log = true) override;
    virtual bool addPiece(enum PieceType type, double x1, double y1,
                          double x2, double y2) override;
    virtual enum ngsEditDeleteResult deletePiece(enum PieceType type) override;

protected:
    virtual bool isNearestSelected(const OGRRawPoint &pt,
                                   double tolerance) override;
    virtual void updateSelectedPoint(const OGRRawPoint &pt,
                                     bool log = false) override;
    virtual ngsPointId selectNearestPoint(const OGRRawPoint &pt,
                                          double tolerance) override;
    virtual GEOSGeom toGEOSGeometry(GEOSContextHandlePtr handle) const override;

protected:
    void insertPoint(int index, const OGRRawPoint &pt);
};

} // namespace ngs


#endif // NGSGEOMETRY_H
