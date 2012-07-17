#
# jVectorMap version 0.2.3
#
# Copyright 2011-2012, Kirill Lebedev
# Licensed under the MIT license.
#

import argparse
import sys
from osgeo import ogr
from osgeo import osr
import anyjson
import shapely.geometry
import codecs


class Map:
  width = 0
  height = 0
  bbox = []

  def __init__(self, name, language):
    self.paths = {}
    self.name = name
    self.language = language

  def addPath(self, path, code, name):
    self.paths[code] = {"path": path, "name": name}

  def getJSCode(self):
    map = {"paths": self.paths, "width": self.width, "height": self.height, "insets": self.insets, "projection": self.projection}
    return "$.fn.vectorMap('addMap', '"+self.name+"_"+self.language+"',"+anyjson.serialize(map)+');'


class Converter:
  def __init__(self, inputFile, **kwargs):
    self.map = Map(kwargs['name'], kwargs['language'])
    self.inputFile = inputFile
    self.features = {}
    self.where = kwargs['where']
    self.codes_file = kwargs['codes_file']
    self.width = kwargs['width']
    self.minimal_area = kwargs['minimal_area']
    self.country_name_index = kwargs['country_name_index']
    self.country_code_index = kwargs['country_code_index']
    self.longtitude0 = kwargs['longtitude0']
    self.inputFileEncoding = kwargs['input_file_encoding']
    if kwargs['viewport']:
      self.viewport = map(lambda s: float(s), kwargs['viewport'].split(' '))
    else:
      self.viewport = False

    # spatial reference to convert to
    self.spatialRef = osr.SpatialReference()
    self.spatialRef.ImportFromProj4('+proj=mill +lat_0=0 +lon_0='+self.longtitude0+' +x_0=0 +y_0=0 +R_A +ellps=WGS84 +datum=WGS84 +units=m +no_defs')

    # handle map insets
    if kwargs['insets']:
      self.insets = anyjson.deserialize(kwargs['insets'])
    else:
      self.insets = []


  def loadData(self):
    source = ogr.Open( self.inputFile )
    layer = source.GetLayer(0)
    layer.SetAttributeFilter( self.where )

    if self.viewport:
      layer.SetSpatialFilterRect( *self.viewport )
      transformation = osr.CoordinateTransformation( layer.GetSpatialRef(), self.spatialRef )
      point1 = transformation.TransformPoint(self.viewport[0], self.viewport[1])
      point2 = transformation.TransformPoint(self.viewport[2], self.viewport[3])
      self.viewportRect = shapely.geometry.box(point1[0], point1[1], point2[0], point2[1])
    else:
      self.viewportRect = False

    layer.ResetReading()

    # load codes from external tsv file if present or geodata file otherwise
    self.codes = {}
    if self.codes_file:
      for line in codecs.open(self.codes_file, 'r', "utf-8"):
        row = map(lambda s: s.strip(), line.split('\t'))
        self.codes[row[1]] = row[0]
    else:
      nextCode = 0
      for feature in layer:
        code = feature.GetFieldAsString(self.country_code_index)
        if code == '-99':
          code = '_'+str(nextCode)
          nextCode += 1
        name = feature.GetFieldAsString(self.country_name_index).decode(self.inputFileEncoding)
        self.codes[name] = code
      layer.ResetReading()

    # load features
    for feature in layer:
      geometry = feature.GetGeometryRef()
      geometryType = geometry.GetGeometryType()

      if geometryType == ogr.wkbPolygon or geometryType == ogr.wkbMultiPolygon:
        geometry.TransformTo( self.spatialRef )
        shapelyGeometry = shapely.wkb.loads( geometry.ExportToWkb() )
        if not shapelyGeometry.is_valid:
          #buffer to fix selfcrosses
          shapelyGeometry = shapelyGeometry.buffer(0)
        shapelyGeometry = self.applyFilters(shapelyGeometry)
        if shapelyGeometry:
          name = feature.GetFieldAsString(self.country_name_index).decode(self.inputFileEncoding)
          code = self.codes[name]
          self.features[code] = {"geometry": shapelyGeometry, "name": name, "code": code}
      else:
        raise Exception, "Wrong geomtry type: "+geometryType


  def convert(self, outputFile):
    self.loadData()

    codes = self.features.keys()
    self.map.insets = []
    envelope = []
    for inset in self.insets:
      insetBbox = self.renderMapInset(inset['codes'], inset['left'], inset['top'], inset['width'])
      insetHeight = (insetBbox[3] - insetBbox[1]) * (inset['width'] / (insetBbox[2] - insetBbox[0]))
      self.map.insets.append({
        "bbox": [{"x": insetBbox[0], "y": -insetBbox[3]}, {"x": insetBbox[2], "y": -insetBbox[1]}],
        "left": inset['left'],
        "top": inset['top'],
        "width": inset['width'],
        "height": insetHeight
      })
      envelope.append(
        shapely.geometry.box(
          inset['left'], inset['top'], inset['left'] + inset['width'], inset['top'] + insetHeight
        )
      )
      for code in inset['codes']:
        codes.remove(code)

    insetBbox = self.renderMapInset(codes, 0, 0, self.width)
    insetHeight = (insetBbox[3] - insetBbox[1]) * (self.width / (insetBbox[2] - insetBbox[0]))

    envelope.append( shapely.geometry.box( 0, 0, self.width, insetHeight ) )
    mapBbox = shapely.geometry.MultiPolygon( envelope ).bounds

    self.map.width = mapBbox[2] - mapBbox[0]
    self.map.height = mapBbox[3] - mapBbox[1]
    self.map.insets.append({
      "bbox": [{"x": insetBbox[0], "y": -insetBbox[3]}, {"x": insetBbox[2], "y": -insetBbox[1]}],
      "left": 0,
      "top": 0,
      "width": self.width,
      "height": insetHeight
    })
    self.map.projection = {"type": 'miller', "centralMeridian": float(self.longtitude0)}

    open(outputFile, 'w').write( self.map.getJSCode() )


  def renderMapInset(self, codes, left, top, width):
    envelope = []
    for code in codes:
      envelope.append( self.features[code]['geometry'].envelope )

    bbox = shapely.geometry.MultiPolygon( envelope ).bounds

    scale = (bbox[2]-bbox[0]) / width

    # generate SVG paths
    for code in codes:
      feature = self.features[code]
      geometry = feature['geometry']
      if args.buffer_distance:
        geometry = geometry.buffer(args.buffer_distance)
      if geometry.is_empty:
        continue
      if args.simplify_tolerance:
        geometry = geometry.simplify(args.simplify_tolerance, preserve_topology=True)
      if isinstance(geometry, shapely.geometry.multipolygon.MultiPolygon):
        polygons = geometry.geoms
      else:
        polygons = [geometry]
      path = ''
      for polygon in polygons:
        rings = []
        rings.append(polygon.exterior)
        rings.extend(polygon.interiors)
        for ring in rings:
          for pointIndex in range( len(ring.coords) ):
            point = ring.coords[pointIndex]
            if pointIndex == 0:
              path += 'M'+str( round( (point[0]-bbox[0]) / scale + left, 2) )
              path += ','+str( round( (bbox[3] - point[1]) / scale + top, 2) )
            else:
              path += 'l' + str( round(point[0]/scale - ring.coords[pointIndex-1][0]/scale, 2) )
              path += ',' + str( round(ring.coords[pointIndex-1][1]/scale - point[1]/scale, 2) )
          path += 'Z'
      self.map.addPath(path, feature['code'], feature['name'])
    return bbox


  def applyFilters(self, geometry):
    if self.viewportRect:
      geometry = self.filterByViewport(geometry)
      if not geometry:
        return False
    if self.minimal_area:
      geometry = self.filterByMinimalArea(geometry)
      if not geometry:
        return False
    return geometry


  def filterByViewport(self, geometry):
    try:
      return geometry.intersection(self.viewportRect)
    except shapely.geos.TopologicalError:
      return False


  def filterByMinimalArea(self, geometry):
    if isinstance(geometry, shapely.geometry.multipolygon.MultiPolygon):
      polygons = geometry.geoms
    else:
      polygons = [geometry]
    polygons = filter(lambda p: p.area > self.minimal_area, polygons)
    return shapely.geometry.multipolygon.MultiPolygon(polygons)


parser = argparse.ArgumentParser(conflict_handler='resolve')
parser.add_argument('input_file')
parser.add_argument('output_file')
parser.add_argument('--scale', type=float)
parser.add_argument('--country_code_index', default=0, type=int)
parser.add_argument('--country_name_index', default=1, type=int)
parser.add_argument('--codes_file', default='', type=str)
parser.add_argument('--where', default='', type=str)
parser.add_argument('--width', type=float)
parser.add_argument('--insets', type=str)
parser.add_argument('--minimal_area', type=float)
parser.add_argument('--buffer_distance', type=float)
parser.add_argument('--simplify_tolerance', type=float)
parser.add_argument('--viewport', type=str)
parser.add_argument('--longtitude0', type=str, default='0')
parser.add_argument('--name', type=str, default='world')
parser.add_argument('--language', type=str, default='en')
parser.add_argument('--input_file_encoding', type=str, default='iso-8859-1')
args = parser.parse_args()

converter = Converter(args.input_file,
  where = args.where,
  codes_file = args.codes_file,
  insets = args.insets,
  width = args.width,
  viewport = args.viewport,
  minimal_area = args.minimal_area,
  country_name_index = args.country_name_index,
  country_code_index = args.country_code_index,
  longtitude0 = args.longtitude0,
  name = args.name,
  language = args.language,
  input_file_encoding = args.input_file_encoding
)
converter.convert(args.output_file)