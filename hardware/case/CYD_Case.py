# SS IR Cloner case v3 for the 2.8in CYD (front shell + back lid). Generated from case_model.py - do not edit by hand.
import adsk.core, adsk.fusion, traceback, json, math, os
S = 0.1                                  # mm -> cm
DATA = json.loads(r"""{"parts":[{"name":"CYD_Case_Front","ops":[["add",{"t":"prism","poly":[[32.0,87.3],[31.9135,88.1779],[31.6575,89.0221],[31.2416,89.8001],[30.682,90.482],[30.0001,91.0416],[29.2221,91.4575],[28.3779,91.7135],[27.5,91.8],[-27.5,91.8],[-28.3779,91.7135],[-29.2221,91.4575],[-30.0001,91.0416],[-30.682,90.482],[-31.2416,89.8001],[-31.6575,89.0221],[-31.9135,88.1779],[-32.0,87.3],[-32.0,4.5],[-31.9135,3.6221],[-31.6575,2.7779],[-31.2416,1.9999],[-30.682,1.318],[-30.0001,0.7584],[-29.2221,0.3425],[-28.3779,0.0865],[-27.5,0.0],[27.5,0.0],[28.3779,0.0865],[29.2221,0.3425],[30.0001,0.7584],[30.682,1.318],[31.2416,1.9999],[31.6575,2.7779],[31.9135,3.6221],[32.0,4.5]],"o":[0,0,1.6],"h":17.7,"ex":[1,0,0],"ey":[0,1,0],"ez":[0,0,1]}],["add",{"t":"prism","poly":[[31.2,87.3],[31.1289,88.0218],[30.9184,88.7159],[30.5764,89.3556],[30.1163,89.9163],[29.5556,90.3764],[28.9159,90.7184],[28.2218,90.9289],[27.5,91.0],[-27.5,91.0],[-28.2218,90.9289],[-28.9159,90.7184],[-29.5556,90.3764],[-30.1163,89.9163],[-30.5764,89.3556],[-30.9184,88.7159],[-31.1289,88.0218],[-31.2,87.3],[-31.2,4.5],[-31.1289,3.7782],[-30.9184,3.0841],[-30.5764,2.4444],[-30.1163,1.8837],[-29.5556,1.4236],[-28.9159,1.0816],[-28.2218,0.8711],[-27.5,0.8],[27.5,0.8],[28.2218,0.8711],[28.9159,1.0816],[29.5556,1.4236],[30.1163,1.8837],[30.5764,2.4444],[30.9184,3.0841],[31.1289,3.7782],[31.2,4.5]],"o":[0,0,19.2],"h":0.9,"ex":[1,0,0],"ey":[0,1,0],"ez":[0,0,1]}],["sub",{"t":"prism","poly":[[30.0,87.3],[29.952,87.7877],[29.8097,88.2567],[29.5787,88.6889],[29.2678,89.0678],[28.8889,89.3787],[28.4567,89.6097],[27.9877,89.752],[27.5,89.8],[-27.5,89.8],[-27.9877,89.752],[-28.4567,89.6097],[-28.8889,89.3787],[-29.2678,89.0678],[-29.5787,88.6889],[-29.8097,88.2567],[-29.952,87.7877],[-30.0,87.3],[-30.0,4.5],[-29.952,4.0123],[-29.8097,3.5433],[-29.5787,3.1111],[-29.2678,2.7322],[-28.8889,2.4213],[-28.4567,2.1903],[-27.9877,2.048],[-27.5,2.0],[27.5,2.0],[27.9877,2.048],[28.4567,2.1903],[28.8889,2.4213],[29.2678,2.7322],[29.5787,3.1111],[29.8097,3.5433],[29.952,4.0123],[30.0,4.5]],"o":[0,0,1.5],"h":16.8,"ex":[1,0,0],"ey":[0,1,0],"ez":[0,0,1]}],["add",{"t":"cyl","a":[-28.1,3.9,1.6],"b":[-28.1,3.9,18.400000000000002],"ra":2.6,"rb":2.6}],["add",{"t":"cyl","a":[28.1,3.9,1.6],"b":[28.1,3.9,18.400000000000002],"ra":2.6,"rb":2.6}],["add",{"t":"cyl","a":[-28.1,87.89999999999999,1.6],"b":[-28.1,87.89999999999999,18.400000000000002],"ra":2.6,"rb":2.6}],["add",{"t":"cyl","a":[28.1,87.89999999999999,1.6],"b":[28.1,87.89999999999999,18.400000000000002],"ra":2.6,"rb":2.6}],["add",{"t":"cyl","a":[-21.0,84.5,14.0],"b":[-21.0,84.5,18.400000000000002],"ra":2.6,"rb":2.6}],["add",{"t":"cyl","a":[21.0,84.5,14.0],"b":[21.0,84.5,18.400000000000002],"ra":2.6,"rb":2.6}],["add",{"t":"cyl","a":[-21.0,6.5,14.0],"b":[-21.0,6.5,18.400000000000002],"ra":2.6,"rb":2.6}],["add",{"t":"cyl","a":[21.0,6.5,14.0],"b":[21.0,6.5,18.400000000000002],"ra":2.6,"rb":2.6}],["add",{"t":"box","lo":[-30.1,42.699999999999996,3.9999999999999996],"hi":[-26.0,43.9,18.400000000000002]}],["add",{"t":"box","lo":[-30.1,53.1,3.9999999999999996],"hi":[-26.0,54.300000000000004,18.400000000000002]}],["sub",{"t":"cyl","a":[-28.1,3.9,1.5],"b":[-28.1,3.9,19.3],"ra":1.4,"rb":1.4}],["sub",{"t":"cyl","a":[28.1,3.9,1.5],"b":[28.1,3.9,19.3],"ra":1.4,"rb":1.4}],["sub",{"t":"cyl","a":[-28.1,87.89999999999999,1.5],"b":[-28.1,87.89999999999999,19.3],"ra":1.4,"rb":1.4}],["sub",{"t":"cyl","a":[28.1,87.89999999999999,1.5],"b":[28.1,87.89999999999999,19.3],"ra":1.4,"rb":1.4}],["sub",{"t":"cyl","a":[-21.0,84.5,13.9],"b":[-21.0,84.5,19.6],"ra":1.4,"rb":1.4}],["sub",{"t":"cyl","a":[21.0,84.5,13.9],"b":[21.0,84.5,19.6],"ra":1.4,"rb":1.4}],["sub",{"t":"cyl","a":[-21.0,6.5,13.9],"b":[-21.0,6.5,19.6],"ra":1.4,"rb":1.4}],["sub",{"t":"cyl","a":[21.0,6.5,13.9],"b":[21.0,6.5,19.6],"ra":1.4,"rb":1.4}],["sub",{"t":"prism","poly":[[23.0,76.95],[22.9712,77.2426],[22.8858,77.524],[22.7472,77.7834],[22.5607,78.0107],[22.3334,78.1972],[22.074,78.3358],[21.7926,78.4212],[21.5,78.45],[-21.5,78.45],[-21.7926,78.4212],[-22.074,78.3358],[-22.3334,78.1972],[-22.5607,78.0107],[-22.7472,77.7834],[-22.8858,77.524],[-22.9712,77.2426],[-23.0,76.95],[-23.0,19.7],[-22.9712,19.4074],[-22.8858,19.126],[-22.7472,18.8666],[-22.5607,18.6393],[-22.3334,18.4528],[-22.074,18.3142],[-21.7926,18.2288],[-21.5,18.2],[21.5,18.2],[21.7926,18.2288],[22.074,18.3142],[22.3334,18.4528],[22.5607,18.6393],[22.7472,18.8666],[22.8858,19.126],[22.9712,19.4074],[23.0,19.7]],"o":[0,0,18.2],"h":2.1,"ex":[1,0,0],"ey":[0,1,0],"ez":[0,0,1]}],["sub",{"t":"prism","poly":[[6.25,12.285],[6.2116,12.6752],[6.0978,13.0504],[5.9129,13.3961],[5.6642,13.6992],[5.3611,13.9479],[5.0154,14.1328],[4.6402,14.2466],[4.25,14.285],[-4.25,14.285],[-4.6402,14.2466],[-5.0154,14.1328],[-5.3611,13.9479],[-5.6642,13.6992],[-5.9129,13.3961],[-6.0978,13.0504],[-6.2116,12.6752],[-6.25,12.285],[-6.25,9.285],[-6.2116,8.8948],[-6.0978,8.5196],[-5.9129,8.1739],[-5.6642,7.8708],[-5.3611,7.6221],[-5.0154,7.4372],[-4.6402,7.3234],[-4.25,7.285],[4.25,7.285],[4.6402,7.3234],[5.0154,7.4372],[5.3611,7.6221],[5.6642,7.8708],[5.9129,8.1739],[6.0978,8.5196],[6.2116,8.8948],[6.25,9.285]],"o":[0,2.2,0],"h":2.4,"ex":[1,0,0],"ey":[0,0,1],"ez":[0,-1,0]}],["sub",{"t":"prism","poly":[[52.3,7.2],[52.2885,7.3171],[52.2543,7.4296],[52.1989,7.5333],[52.1243,7.6243],[52.0333,7.6989],[51.9296,7.7543],[51.8171,7.7885],[51.7,7.8],[45.3,7.8],[45.1829,7.7885],[45.0704,7.7543],[44.9667,7.6989],[44.8757,7.6243],[44.8011,7.5333],[44.7457,7.4296],[44.7115,7.3171],[44.7,7.2],[44.7,6.0],[44.7115,5.8829],[44.7457,5.7704],[44.8011,5.6667],[44.8757,5.5757],[44.9667,5.5011],[45.0704,5.4457],[45.1829,5.4115],[45.3,5.4],[51.7,5.4],[51.8171,5.4115],[51.9296,5.4457],[52.0333,5.5011],[52.1243,5.5757],[52.1989,5.6667],[52.2543,5.7704],[52.2885,5.8829],[52.3,6.0]],"o":[-32.2,0,0],"h":2.4,"ex":[0,1,0],"ey":[0,0,1],"ez":[1,0,0]}],["sub",{"t":"box","lo":[-16.1,89.7,1.5],"hi":[23.159999999999997,91.89999999999999,6.4]}],["sub",{"t":"cyl","a":[-12.05,89.6,6.4],"b":[-12.05,92.0,6.4],"ra":2.75,"rb":2.75}],["sub",{"t":"cyl","a":[-4.550000000000001,89.6,6.4],"b":[-4.550000000000001,92.0,6.4],"ra":2.75,"rb":2.75}],["sub",{"t":"cyl","a":[2.9499999999999993,89.6,6.4],"b":[2.9499999999999993,92.0,6.4],"ra":2.75,"rb":2.75}],["sub",{"t":"cyl","a":[19.109999999999996,89.6,6.4],"b":[19.109999999999996,92.0,6.4],"ra":2.75,"rb":2.75}]],"print_rot":"Z_flip","color":[40,44,52]},{"name":"CYD_Case_Back","ops":[["add",{"t":"prism","poly":[[32.0,87.3],[31.9135,88.1779],[31.6575,89.0221],[31.2416,89.8001],[30.682,90.482],[30.0001,91.0416],[29.2221,91.4575],[28.3779,91.7135],[27.5,91.8],[-27.5,91.8],[-28.3779,91.7135],[-29.2221,91.4575],[-30.0001,91.0416],[-30.682,90.482],[-31.2416,89.8001],[-31.6575,89.0221],[-31.9135,88.1779],[-32.0,87.3],[-32.0,4.5],[-31.9135,3.6221],[-31.6575,2.7779],[-31.2416,1.9999],[-30.682,1.318],[-30.0001,0.7584],[-29.2221,0.3425],[-28.3779,0.0865],[-27.5,0.0],[27.5,0.0],[28.3779,0.0865],[29.2221,0.3425],[30.0001,0.7584],[30.682,1.318],[31.2416,1.9999],[31.6575,2.7779],[31.9135,3.6221],[32.0,4.5]],"o":[0,0,0],"h":1.6,"ex":[1,0,0],"ey":[0,1,0],"ez":[0,0,1]}],["add",{"t":"prism","poly":[[29.65,87.3],[29.6087,87.7194],[29.4863,88.1228],[29.2877,88.4945],[29.0203,88.8203],[28.6945,89.0877],[28.3228,89.2863],[27.9194,89.4087],[27.5,89.45],[-27.5,89.45],[-27.9194,89.4087],[-28.3228,89.2863],[-28.6945,89.0877],[-29.0203,88.8203],[-29.2877,88.4945],[-29.4863,88.1228],[-29.6087,87.7194],[-29.65,87.3],[-29.65,4.5],[-29.6087,4.0806],[-29.4863,3.6772],[-29.2877,3.3055],[-29.0203,2.9797],[-28.6945,2.7123],[-28.3228,2.5137],[-27.9194,2.3913],[-27.5,2.35],[27.5,2.35],[27.9194,2.3913],[28.3228,2.5137],[28.6945,2.7123],[29.0203,2.9797],[29.2877,3.3055],[29.4863,3.6772],[29.6087,4.0806],[29.65,4.5]],"o":[0,0,1.5],"h":2.1,"ex":[1,0,0],"ey":[0,1,0],"ez":[0,0,1]}],["sub",{"t":"prism","poly":[[28.45,87.3],[28.4317,87.4853],[28.3777,87.6635],[28.2899,87.8278],[28.1718,87.9718],[28.0278,88.0899],[27.8635,88.1777],[27.6853,88.2317],[27.5,88.25],[-27.5,88.25],[-27.6853,88.2317],[-27.8635,88.1777],[-28.0278,88.0899],[-28.1718,87.9718],[-28.2899,87.8278],[-28.3777,87.6635],[-28.4317,87.4853],[-28.45,87.3],[-28.45,4.5],[-28.4317,4.3147],[-28.3777,4.1365],[-28.2899,3.9722],[-28.1718,3.8282],[-28.0278,3.7101],[-27.8635,3.6223],[-27.6853,3.5683],[-27.5,3.55],[27.5,3.55],[27.6853,3.5683],[27.8635,3.6223],[28.0278,3.7101],[28.1718,3.8282],[28.2899,3.9722],[28.3777,4.1365],[28.4317,4.3147],[28.45,4.5]],"o":[0,0,1.6],"h":2.2,"ex":[1,0,0],"ey":[0,1,0],"ez":[0,0,1]}],["sub",{"t":"box","lo":[-16.1,87.8,1.6],"hi":[23.159999999999997,89.89999999999999,3.9]}],["add",{"t":"box","lo":[-15.8,89.8,1.5],"hi":[22.859999999999996,91.8,6.25]}],["add",{"t":"cyl","a":[-13.05,59.8,1.5],"b":[-13.05,59.8,3.4000000000000004],"ra":3.0,"rb":3.0}],["add",{"t":"cyl","a":[21.95,82.3,1.5],"b":[21.95,82.3,3.4000000000000004],"ra":3.0,"rb":3.0}],["add",{"t":"cyl","a":[21.95,82.3,3.3000000000000003],"b":[21.95,82.3,4.9],"ra":1.35,"rb":1.35}],["add",{"t":"box","lo":[-8.5,12.299999999999997,1.5],"hi":[-1.2999999999999998,13.499999999999996,4.6]}],["add",{"t":"box","lo":[-8.5,12.299999999999997,1.5],"hi":[-7.3,19.499999999999996,4.6]}],["add",{"t":"box","lo":[-8.5,55.099999999999994,1.5],"hi":[-1.2999999999999998,56.3,1.7000000000000004]}],["add",{"t":"box","lo":[-8.5,49.099999999999994,1.5],"hi":[-7.3,56.3,1.7000000000000004]}],["add",{"t":"box","lo":[13.3,12.299999999999997,1.5],"hi":[20.5,13.499999999999996,4.6]}],["add",{"t":"box","lo":[19.3,12.299999999999997,1.5],"hi":[20.5,19.499999999999996,4.6]}],["add",{"t":"box","lo":[13.3,55.099999999999994,1.5],"hi":[20.5,56.3,1.7000000000000004]}],["add",{"t":"box","lo":[19.3,49.099999999999994,1.5],"hi":[20.5,56.3,1.7000000000000004]}],["sub",{"t":"cyl","a":[-28.1,3.9,1.55],"b":[-28.1,3.9,3.9],"ra":3.0,"rb":3.0}],["sub",{"t":"cyl","a":[-28.1,3.9,-0.1],"b":[-28.1,3.9,1.7000000000000002],"ra":1.8,"rb":1.8}],["sub",{"t":"cyl","a":[-28.1,3.9,-0.1],"b":[-28.1,3.9,1.2],"ra":3.2,"rb":1.8}],["sub",{"t":"cyl","a":[28.1,3.9,1.55],"b":[28.1,3.9,3.9],"ra":3.0,"rb":3.0}],["sub",{"t":"cyl","a":[28.1,3.9,-0.1],"b":[28.1,3.9,1.7000000000000002],"ra":1.8,"rb":1.8}],["sub",{"t":"cyl","a":[28.1,3.9,-0.1],"b":[28.1,3.9,1.2],"ra":3.2,"rb":1.8}],["sub",{"t":"cyl","a":[-28.1,87.89999999999999,1.55],"b":[-28.1,87.89999999999999,3.9],"ra":3.0,"rb":3.0}],["sub",{"t":"cyl","a":[-28.1,87.89999999999999,-0.1],"b":[-28.1,87.89999999999999,1.7000000000000002],"ra":1.8,"rb":1.8}],["sub",{"t":"cyl","a":[-28.1,87.89999999999999,-0.1],"b":[-28.1,87.89999999999999,1.2],"ra":3.2,"rb":1.8}],["sub",{"t":"cyl","a":[28.1,87.89999999999999,1.55],"b":[28.1,87.89999999999999,3.9],"ra":3.0,"rb":3.0}],["sub",{"t":"cyl","a":[28.1,87.89999999999999,-0.1],"b":[28.1,87.89999999999999,1.7000000000000002],"ra":1.8,"rb":1.8}],["sub",{"t":"cyl","a":[28.1,87.89999999999999,-0.1],"b":[28.1,87.89999999999999,1.2],"ra":3.2,"rb":1.8}],["sub",{"t":"cyl","a":[-13.05,59.8,-0.1],"b":[-13.05,59.8,3.5000000000000004],"ra":1.8,"rb":1.8}],["sub",{"t":"cyl","a":[-13.05,59.8,-0.1],"b":[-13.05,59.8,1.2],"ra":3.2,"rb":1.8}],["sub",{"t":"cyl","a":[11.57,5.760000000000005,-0.1],"b":[11.57,5.760000000000005,1.7000000000000002],"ra":1.1,"rb":1.1}],["sub",{"t":"cyl","a":[-11.57,5.760000000000005,-0.1],"b":[-11.57,5.760000000000005,1.7000000000000002],"ra":1.1,"rb":1.1}],["sub",{"t":"cyl","a":[-12.05,89.6,6.4],"b":[-12.05,92.0,6.4],"ra":2.75,"rb":2.75}],["sub",{"t":"cyl","a":[-4.550000000000001,89.6,6.4],"b":[-4.550000000000001,92.0,6.4],"ra":2.75,"rb":2.75}],["sub",{"t":"cyl","a":[2.9499999999999993,89.6,6.4],"b":[2.9499999999999993,92.0,6.4],"ra":2.75,"rb":2.75}],["sub",{"t":"cyl","a":[19.109999999999996,89.6,6.4],"b":[19.109999999999996,92.0,6.4],"ra":2.75,"rb":2.75}]],"print_rot":null,"color":[70,76,88]}],"texts":[],"T":20.1}""")
DISPLAY_OFFSET = {"CYD_Case_Back": (0, 0, -40)}
PLATE = {"CYD_Case_Front": (-45, 0), "CYD_Case_Back": (45, 0)}
ROTS = {"Z_flip": [[1, 0, 0], [0, -1, 0], [0, 0, -1]], None: None}

def P(p):
    return adsk.core.Point3D.create(p[0] * S, p[1] * S, p[2] * S)

def run(context):
    app = adsk.core.Application.get(); ui = app.userInterface
    try:
        app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType)
        design = adsk.fusion.Design.cast(app.activeProduct)
        design.designType = adsk.fusion.DesignTypes.DirectDesignType
        design.fusionUnitsManager.distanceDisplayUnits = adsk.fusion.DistanceUnits.MillimeterDistanceUnits
        root = design.rootComponent; tbm = adsk.fusion.TemporaryBRepManager.get(); warn = []

        def prism(pr):
            sk = root.sketches.add(root.xYConstructionPlane); sk.isComputeDeferred = True
            pts = [adsk.core.Point3D.create(x * S, y * S, 0) for x, y in pr["poly"]]
            L = sk.sketchCurves.sketchLines; first = prev = L.addByTwoPoints(pts[0], pts[1])
            for q in pts[2:]:
                prev = L.addByTwoPoints(prev.endSketchPoint, q)
            L.addByTwoPoints(prev.endSketchPoint, first.startSketchPoint); sk.isComputeDeferred = False
            prof = max((sk.profiles.item(i) for i in range(sk.profiles.count)),
                       key=lambda p: p.areaProperties(adsk.fusion.CalculationAccuracy.LowCalculationAccuracy).area)
            ext = root.features.extrudeFeatures.addSimple(prof, adsk.core.ValueInput.createByReal(pr["h"] * S),
                                                          adsk.fusion.FeatureOperations.NewBodyFeatureOperation)
            b = tbm.copy(ext.bodies.item(0)); ext.bodies.item(0).deleteMe(); sk.deleteMe()
            m = adsk.core.Matrix3D.create()
            m.setWithCoordinateSystem(P(pr["o"]), *(adsk.core.Vector3D.create(*pr[k]) for k in ("ex", "ey", "ez")))
            tbm.transform(b, m); return b

        def make(pr):
            t = pr["t"]
            if t == "cyl":
                return tbm.createCylinderOrCone(P(pr["a"]), pr["ra"] * S, P(pr["b"]), pr["rb"] * S)
            if t == "box":
                lo, hi = pr["lo"], pr["hi"]; c = [(lo[i] + hi[i]) / 2 for i in range(3)]
                return tbm.createBox(adsk.core.OrientedBoundingBox3D.create(P(c), adsk.core.Vector3D.create(1, 0, 0),
                       adsk.core.Vector3D.create(0, 1, 0), (hi[0] - lo[0]) * S, (hi[1] - lo[1]) * S, (hi[2] - lo[2]) * S))
            if t == "prism":
                return prism(pr)

        def boolean(body, b, op, label):
            kind = adsk.fusion.BooleanTypes.UnionBooleanType if op == "add" else adsk.fusion.BooleanTypes.DifferenceBooleanType
            for eps in (0.0, 0.0004, -0.0007, 0.0011):     # retry with a micro-nudge if the boolean fails
                bb = tbm.copy(b)
                if eps:
                    m = adsk.core.Matrix3D.create(); m.translation = adsk.core.Vector3D.create(eps, eps * .7, eps * .4); tbm.transform(bb, m)
                trial = tbm.copy(body)
                if tbm.booleanOperation(trial, bb, kind):
                    return trial
            warn.append(label + ": boolean failed")
            return body

        def text_bodies(tx):
            # engraving tool: text sketched at z=0, extruded, moved to the front face
            sk = root.sketches.add(root.xYConstructionPlane)
            inp = sk.sketchTexts.createInput2(tx["text"], tx["h"] * S)
            c1 = adsk.core.Point3D.create((tx["cx"] - tx["w"] / 2) * S, (tx["cy"] - tx["h"]) * S, 0)
            c2 = adsk.core.Point3D.create((tx["cx"] + tx["w"] / 2) * S, (tx["cy"] + tx["h"]) * S, 0)
            inp.setAsMultiLine(c1, c2, adsk.core.HorizontalAlignments.CenterHorizontalAlignment,
                               adsk.core.VerticalAlignments.MiddleVerticalAlignment, 0)
            st = sk.sketchTexts.add(inp)
            ext = root.features.extrudeFeatures.addSimple(st, adsk.core.ValueInput.createByReal((tx["depth"] + 0.2) * S),
                                                          adsk.fusion.FeatureOperations.NewBodyFeatureOperation)
            out = []
            m = adsk.core.Matrix3D.create(); m.translation = adsk.core.Vector3D.create(0, 0, (DATA["T"] - tx["depth"]) * S)
            for i in range(ext.bodies.count):
                b = tbm.copy(ext.bodies.item(i)); tbm.transform(b, m); out.append(b)
            for i in range(ext.bodies.count - 1, -1, -1):
                ext.bodies.item(i).deleteMe()
            sk.deleteMe()
            return out

        def build(part):
            body = None
            for n, (op, pr) in enumerate(part["ops"]):
                b = make(pr)
                if body is None:
                    body = b if op == "add" else None; continue
                body = boolean(body, b, op, "%s op %d" % (part["name"], n))
            for tx in DATA["texts"]:
                if tx["part"] != part["name"]:
                    continue
                try:
                    for b in text_bodies(tx):
                        body = boolean(body, b, "sub", "%s text '%s'" % (part["name"], tx["text"]))
                except Exception:
                    warn.append("text '%s' skipped: %s" % (tx["text"], traceback.format_exc().splitlines()[-1]))
            return body

        out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "STL_OUT_CYD"); os.makedirs(out_dir, exist_ok=True)
        plate = root.occurrences.addNewComponent(adsk.core.Matrix3D.create()).component; plate.name = "Print plate"
        parts = DATA["parts"]
        prog = ui.createProgressDialog(); prog.show("SS IR Cloner case v3 (CYD)", "%v / %m", 0, len(parts), 0); n_ok = 0; report = []
        for i, part in enumerate(parts):
            prog.progressValue = i; adsk.doEvents()
            body = build(part)
            if body is None:
                warn.append(part["name"] + ": empty"); continue
            comp = root.occurrences.addNewComponent(adsk.core.Matrix3D.create()).component; comp.name = part["name"]
            disp_b = tbm.copy(body); off = DISPLAY_OFFSET.get(part["name"])
            if off:   # assembly view: show the halves apart (does not change the exported STL)
                m = adsk.core.Matrix3D.create(); m.translation = adsk.core.Vector3D.create(off[0] * S, off[1] * S, off[2] * S); tbm.transform(disp_b, m)
            rb0 = comp.bRepBodies.add(disp_b); rb0.name = part["name"]
            pb = tbm.copy(body); R = ROTS.get(part.get("print_rot"))
            if R:
                m = adsk.core.Matrix3D.create(); [m.setCell(r, c, R[r][c]) for r in range(3) for c in range(3)]; tbm.transform(pb, m)
            bb = pb.boundingBox; cx, cy = PLATE.get(part["name"], (0, 0))
            m = adsk.core.Matrix3D.create(); m.translation = adsk.core.Vector3D.create(cx * S - (bb.minPoint.x + bb.maxPoint.x) / 2,
                                                                                       cy * S - (bb.minPoint.y + bb.maxPoint.y) / 2, -bb.minPoint.z)
            tbm.transform(pb, m); rb = plate.bRepBodies.add(pb); rb.name = part["name"] + "_print"
            opt = design.exportManager.createSTLExportOptions(rb, os.path.join(out_dir, part["name"] + ".stl"))
            opt.meshRefinement = adsk.fusion.MeshRefinementSettings.MeshRefinementHigh; opt.isBinaryFormat = True
            design.exportManager.execute(opt); n_ok += 1
            report.append("%s: %.1f cm3" % (part["name"], rb0.volume))
        plate.isBodiesFolderLightBulbOn = False
        prog.hide(); app.activeViewport.fit()
        ui.messageBox("Built %d parts, exported %d STL to:\n%s\n\n%s\n\nWarnings:\n%s" % (
            len(parts), n_ok, os.path.abspath(out_dir), "\n".join(report), "\n".join(warn[:20]) or "none"))
    except Exception:
        ui.messageBox("Failed:\n" + traceback.format_exc())
