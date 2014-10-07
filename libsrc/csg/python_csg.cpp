#ifdef NG_PYTHON

#include <boost/python.hpp>
#include <csg.hpp>


using namespace netgen;
namespace bp = boost::python;


//////////////////////////////////////////////////////////////////////
// Lambda to function pointer conversion
template <typename Function>
struct function_traits
  : public function_traits<decltype(&Function::operator())> {};

template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType(ClassType::*)(Args...) const> {
  typedef ReturnType (*pointer)(Args...);
  typedef ReturnType return_type;
};

template <typename Function>
typename function_traits<Function>::pointer
FunctionPointer (const Function& lambda) {
  return static_cast<typename function_traits<Function>::pointer>(lambda);
}


template <class T>
inline string ToString (const T& t)
{
  stringstream ss;
  ss << t;
  return ss.str();
}

// a shadow solid tree using shared pointers.

class SPSolid
{
  shared_ptr<SPSolid> s1, s2;
  Solid * solid;
  int bc = -1;
  string material;
  bool owner;
public:
  enum optyp { TERM, SECTION, UNION, SUB };

  SPSolid (Solid * as) : solid(as), owner(true), op(TERM) { ; }
  ~SPSolid () 
  {
    ; // if (owner) delete solid;
  }  
  SPSolid (optyp aop, shared_ptr<SPSolid> as1, shared_ptr<SPSolid> as2) 
    : s1(as1), s2(as2), owner(true), op(aop) 
  { 
    if (aop == UNION)
      solid = new Solid (Solid::UNION, s1->GetSolid(), s2->GetSolid());
    else if (aop == SECTION)
      solid = new Solid (Solid::SECTION, s1->GetSolid(), s2->GetSolid());
    else if (aop == SUB)
      solid = new Solid (Solid::SUB, s1->GetSolid()); // , s2->GetSolid());
  }

  Solid * GetSolid() { return solid; }
  const Solid * GetSolid() const { return solid; }

  void GiveUpOwner() 
  { 
    owner = false; 
    if (s1) s1 -> GiveUpOwner();
    if (s2) s2 -> GiveUpOwner();
  }

  void AddSurfaces(CSGeometry & geom)
  {
    if (op == TERM)
      geom.AddSurfaces (solid->GetPrimitive());
    if (s1) s1 -> AddSurfaces (geom);
    if (s2) s2 -> AddSurfaces (geom);
  }

  void SetMaterial (string mat)  { material = mat; }

  string GetMaterial ()
  {
    if (!material.empty()) return material;
    if (s1)
      {
        string s1mat = s1->GetMaterial();
        if (!s1mat.empty()) return s1mat;
      }
    if (s2)
      {
        string s2mat = s2->GetMaterial();
        if (!s2mat.empty()) return s2mat;
      }
    return material;
  }

  void SetBC(int abc) 
  {
    if (bc == -1) 
      {
        bc = abc;
        if (s1) s1 -> SetBC(bc);
        if (s2) s2 -> SetBC(bc);
        if (op == TERM)
          {
            Primitive * prim = solid -> GetPrimitive();
            for (int i = 0; i < prim->GetNSurfaces(); i++)
              prim->GetSurface(i).SetBCProperty (abc);
            cout << "set " << prim->GetNSurfaces() << " surfaces to bc " << bc << endl;
          }
      }
  }
private:
  optyp op;
};

inline ostream & operator<< (ostream & ost, const SPSolid & sol)
{
  ost << *sol.GetSolid();
  return ost;
}

namespace netgen
{
  extern CSGeometry * ParseCSG (istream & istr);
}

void ExportCSG() 
{
  
  std::string nested_name = "csg";
  if( bp::scope() )
    nested_name = bp::extract<std::string>(bp::scope().attr("__name__") + ".csg");
                                           
  bp::object module(bp::handle<>(bp::borrowed(PyImport_AddModule(nested_name.c_str()))));
  
  cout << "exporting csg " << nested_name << endl;
  bp::object parent = bp::scope() ? bp::scope() : bp::import("__main__");
  parent.attr("csg") = module ;
  
  bp::scope local_scope(module);


  bp::class_<Point<2>> ("Point2d", bp::init<double,double>()) 
    .def(bp::self+Vec<2>())
    ;

  bp::class_<Point<3>> ("Point3d", bp::init<double,double,double>()) 
    .def(bp::self-bp::self)
    .def(bp::self+Vec<3>())
    .def(bp::self-Vec<3>())
    ;

  bp::def ("Pnt", FunctionPointer( [] (double x, double y, double z) { return Point<3>(x,y,z); } ) );
  bp::def ("Pnt", FunctionPointer( [] (double x, double y) { return Point<2>(x,y); } ) );

  bp::class_<Vec<2>> ("Vec2d", bp::init<double,double>()) 
    .def(bp::self+bp::self)
//     .def(bp::self*double())
    .def(double()*bp::self)
    ;

  bp::class_<Vec<3>> ("Vec3d", bp::init<double,double,double>()) 
    .def(bp::self+bp::self)
//     .def(bp::self*double())
    .def(double()*bp::self)
    ;

  bp::def ("Vec", FunctionPointer( [] (double x, double y, double z) { return Vec<3>(x,y,z); } ) );
  bp::def ("Vec", FunctionPointer( [] (double x, double y) { return Vec<2>(x,y); } ) );

    

  bp::class_<SPSolid, shared_ptr<SPSolid>, boost::noncopyable> ("Solid", bp::no_init)
    .def ("__str__", &ToString<SPSolid>)
    .def ("__add__", FunctionPointer( [] ( shared_ptr<SPSolid> self, shared_ptr<SPSolid> other ) { return make_shared<SPSolid> (SPSolid::UNION, self, other); } ) )
    .def ("__mul__", FunctionPointer( [] ( shared_ptr<SPSolid> self, shared_ptr<SPSolid> other ) { return make_shared<SPSolid> (SPSolid::SECTION, self, other); } ) )
    .def ("__sub__", FunctionPointer( [] ( shared_ptr<SPSolid> self, shared_ptr<SPSolid> other ) 
                                      { return make_shared<SPSolid> (SPSolid::SECTION, self, make_shared<SPSolid> (SPSolid::SUB, other, nullptr)); } ) )
//     .def ("__neg__", FunctionPointer( [] ( shared_ptr<SPSolid> self ) { return make_shared<SPSolid> (SPSolid::SUB, self); } ) ) COMPLEMENT?

    .def ("bc", FunctionPointer([](shared_ptr<SPSolid> & self, int nr) -> shared_ptr<SPSolid> 
                                { self->SetBC(nr); return self; }))
    .def ("mat", FunctionPointer([](shared_ptr<SPSolid> & self, string mat) -> shared_ptr<SPSolid> 
                                 { self->SetMaterial(mat); return self; }))
    .def ("mat", &SPSolid::GetMaterial)
    ;

  bp::def ("Sphere", FunctionPointer([](Point<3> c, double r)
                                     {
                                       Sphere * sp = new Sphere (c, r);
                                       Solid * sol = new Solid (sp);
                                       return make_shared<SPSolid> (sol);
                                     }));
  bp::def ("Plane", FunctionPointer([](Point<3> p, Vec<3> n)
                                    {
                                      Plane * sp = new Plane (p,n);
                                      Solid * sol = new Solid (sp);
                                      return make_shared<SPSolid> (sol);
                                    }));
  bp::def ("Cylinder", FunctionPointer([](Point<3> a, Point<3> b, double r)
                                       {
                                         Cylinder * cyl = new Cylinder (a, b, r);
                                         Solid * sol = new Solid (cyl);
                                         return make_shared<SPSolid> (sol);
                                       }));
  bp::def ("OrthoBrick", FunctionPointer([](Point<3> p1, Point<3> p2)
                                         {
                                           OrthoBrick * brick = new OrthoBrick (p1,p2);
                                           Solid * sol = new Solid (brick);
                                           return make_shared<SPSolid> (sol);
                                         }));
  
  bp::def ("Or", FunctionPointer([](shared_ptr<SPSolid> s1, shared_ptr<SPSolid> s2)
                                 {
                                   return make_shared<SPSolid> (SPSolid::UNION, s1, s2);
                                 }));
  bp::def ("And", FunctionPointer([](shared_ptr<SPSolid> s1, shared_ptr<SPSolid> s2)
                                  {
                                    return make_shared<SPSolid> (SPSolid::SECTION, s1, s2);
                                  }));


  bp::class_<CSGeometry, boost::noncopyable> ("CSGeometry")
    .def("__init__", bp::make_constructor (FunctionPointer
                                           ([](const string & filename)
                                            {
                                              cout << "load geometry";
                                              ifstream ist(filename);
                                              shared_ptr<CSGeometry> geom(ParseCSG(ist));
                                              geom -> FindIdenticSurfaces(1e-8 * geom->MaxSize()); 
                                              return geom;
                                            })))

    .def("__init__", bp::make_constructor (FunctionPointer
                                           ([](const bp::list & solidlist)
                                            {
                                              cout << "csg from list";
                                              auto geom = make_shared<CSGeometry>();
                                              for (int i = 0; i < len(solidlist); i++)
                                                {
                                                  bp::object obj = solidlist[i];
                                                  cout << "obj " << i << endl;

                                                  bp::extract<shared_ptr<SPSolid>> solid(solidlist[i]);
                                                  if(solid.check())
                                                    {
                                                      cout << "its a solid" << endl;
                                                      solid()->AddSurfaces (*geom);
                                                      solid()->GiveUpOwner();
                                                      int tlonr = geom->SetTopLevelObject (solid()->GetSolid());
                                                      geom->GetTopLevelObject(tlonr) -> SetMaterial(solid()->GetMaterial());
                                                    }
                                                }
                                              geom -> FindIdenticSurfaces(1e-8 * geom->MaxSize()); 
                                              return geom;
                                            })))

    .def("Save", FunctionPointer([] (CSGeometry & self, string filename)
                                 {
                                   cout << "save geometry to file " << filename << endl;
                                   self.Save (filename);
                                 }))
    .def("Add", FunctionPointer
         ([] (CSGeometry & self, shared_ptr<SPSolid> solid)
          {
            solid->AddSurfaces (self);
            solid->GiveUpOwner();
            int tlonr = self.SetTopLevelObject (solid->GetSolid());
            self.GetTopLevelObject(tlonr) -> SetMaterial(solid->GetMaterial());
          }))

    .add_property ("ntlo", &CSGeometry::GetNTopLevelObjects)
    ;

  bp::def("GenerateMesh", FunctionPointer
          ([](CSGeometry & geo, MeshingParameters & param)
           {
             // testout = new ofstream ("test.out");
             shared_ptr<Mesh> dummy;
             cout << "Genrate Mesh, params = " << param << endl;
             cout << "geom, bbox = " << geo.BoundingBox() << endl;
             geo.FindIdenticSurfaces(1e-8 * geo.MaxSize()); 
             geo.GenerateMesh (dummy, param, 0, 6);
             return dummy;
           }))
    ;
  
}





BOOST_PYTHON_MODULE(libcsg) {
  ExportCSG();
}


#endif
