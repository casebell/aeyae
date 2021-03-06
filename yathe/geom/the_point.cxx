// -*- Mode: c++; tab-width: 8; c-basic-offset: 2; indent-tabs-mode: nil -*-
// NOTE: the first line of this file sets up source code indentation rules
// for Emacs; it is also a hint to anyone modifying this file.

// File         : the_point.cxx
// Author       : Pavel Aleksandrovich Koshevoy
// Created      : Sun Jun 23 21:53:36 MDT 2002
// Copyright    : (C) 2002
// License      : MIT
// Description  : The 3D point primitive.

// local includes:
#include "geom/the_curve.hxx"
#include "geom/the_point.hxx"
#include "doc/the_reference.hxx"
#include "math/the_deviation.hxx"
#include "math/the_view_volume.hxx"
#include "sel/the_pick_rec.hxx"

// system includes:
#include <assert.h>


//----------------------------------------------------------------
// the_point_t::intersect
//
bool
the_point_t::
intersect(const the_view_volume_t & volume,
          std::list<the_pick_data_t> & data) const
{
  const p3x1_t wcs_pt = value();

  // shortcuts:
  p3x1_t cyl_pt;
  float & radius = cyl_pt.x();
  float & angle  = cyl_pt.y();
  float & depth  = cyl_pt.z();

  // find the depth of this point within the volume:
  depth = volume.depth_of_wcs_pt(wcs_pt);

  // find the UV frame within the volume at the specified depth:
  the_cyl_uv_csys_t uv_frame;
  volume.uv_frame_at_depth(depth, uv_frame);

  // calculate the polar coordinates of this point within the UV frame:
  uv_frame.wcs_to_lcs(wcs_pt, radius, angle);

  // if the radius of this point within the UV frame exceeds 1.0 then
  // this point is outside the volume - does not intersect:
  if (radius > 1.0)
  {
    return false;
  }

  // this point falls within the volume boundaries:
  data.push_back(the_pick_data_t(cyl_pt, new the_point_ref_t(id())));
  return true;
}

//----------------------------------------------------------------
// the_point_t::save
//
bool
the_point_t::save(std::ostream & stream) const
{
  ::save(stream, anchor_);
  ::save(stream, weight_);
  return the_primitive_t::save(stream);
}

//----------------------------------------------------------------
// the_point_t::load
//
bool
the_point_t::load(std::istream & stream)
{
  ::load(stream, anchor_);
  ::load(stream, weight_);
  return the_primitive_t::load(stream);
}

//----------------------------------------------------------------
// the_point_t::dump
//
void
the_point_t::dump(ostream & strm, unsigned int indent) const
{
  strm << INDSCP << "the_point_t(" << (void *)this << ")" << endl
       << INDSCP << "{" << endl;
  the_primitive_t::dump(strm, INDNXT);
  strm << INDSCP << "}" << endl << endl;
}


//----------------------------------------------------------------
// the_hard_point_t::set_value
//
bool
the_hard_point_t::set_value(const the_view_mgr_t & /* view_mgr */,
                            const p3x1_t & wcs_pt)
{
  value_ = wcs_pt;
  the_graph_node_t::request_regeneration(this);
  return true;
}

//----------------------------------------------------------------
// the_hard_point_t::save
//
bool
the_hard_point_t::save(std::ostream & stream) const
{
  ::save(stream, value_);
  return the_point_t::save(stream);
}

//----------------------------------------------------------------
// the_hard_point_t::load
//
bool
the_hard_point_t::load(std::istream & stream)
{
  ::load(stream, value_);
  return the_point_t::load(stream);
}

//----------------------------------------------------------------
// the_hard_point_t::dump
//
void
the_hard_point_t::dump(ostream & strm, unsigned int indent) const
{
  strm << INDSCP << "the_hard_point_t(" << (void *)this << ")" << endl
       << INDSCP << "{" << endl;
  the_point_t::dump(strm, INDNXT);
  strm << INDSTR << "value_ = " << value_ << ";" << endl
       << INDSCP << "}" << endl << endl;
}


//----------------------------------------------------------------
// the_supported_point_t::the_supported_point_t
//
the_supported_point_t::the_supported_point_t():
  the_point_t(),
  ref_(NULL)
{}

//----------------------------------------------------------------
// the_supported_point_t::the_supported_point_t
//
the_supported_point_t::the_supported_point_t(const the_reference_t & ref):
  the_point_t()
{
  ref_ = ref.clone();
}

//----------------------------------------------------------------
// the_supported_point_t::the_supported_point_t
//
the_supported_point_t::the_supported_point_t(const the_supported_point_t & pt):
  the_point_t(pt),
  ref_(pt.ref_->clone()),
  value_(pt.value_)
{}

//----------------------------------------------------------------
// the_supported_point_t::~the_supported_point_t
//
the_supported_point_t::~the_supported_point_t()
{
  delete ref_;
  ref_ = NULL;
}

//----------------------------------------------------------------
// the_supported_point_t::added_to_the_registry
//
void
the_supported_point_t::added_to_the_registry(the_registry_t * registry,
                                             const unsigned int & id)
{
  the_point_t::added_to_the_registry(registry, id);

  // update the graph:
  establish_supporter_dependent(registry, ref_->id(), id);
}

//----------------------------------------------------------------
// the_supported_point_t::regenerate
//
bool
the_supported_point_t::regenerate()
{
  the_registry_t * r = registry();

  // look up the reference, evaluate the paramater with
  // respect to the reference, return the value:
  return ref_->eval(r, value_);
}

//----------------------------------------------------------------
// the_supported_point_t::reparameterize
//
bool
the_supported_point_t::reparameterize()
{
  the_registry_t * r = registry();
  return ref_->reparameterize(r, value_);
}

//----------------------------------------------------------------
// the_supported_point_t::set_value
//
bool
the_supported_point_t::set_value(const the_view_mgr_t & view_mgr,
                                 const p3x1_t & wcs_pt)
{
  the_registry_t * r = registry();
  bool ok = ref_->move(r, view_mgr, wcs_pt);
  ref_->eval(r, value_);

  if (ok)
  {
    the_graph_node_t::request_regeneration(this);
  }
  return ok;
}

//----------------------------------------------------------------
// the_supported_point_t::symbol
//
the_point_symbol_id_t
the_supported_point_t::symbol() const
{
  return ref_->symbol();
}

//----------------------------------------------------------------
// the_supported_point_t::save
//
bool
the_supported_point_t::save(std::ostream & stream) const
{
  ::save(stream, ref_);
  ::save(stream, value_);
  return the_point_t::save(stream);
}

//----------------------------------------------------------------
// the_supported_point_t::load
//
bool
the_supported_point_t::load(std::istream & stream)
{
  the_graph_node_ref_t * ref = NULL;
  ::load(stream, ref);
  ref_ = dynamic_cast<the_reference_t *>(ref);

  ::load(stream, value_);
  return the_point_t::load(stream);
}

//----------------------------------------------------------------
// the_supported_point_t::dump
//
void
the_supported_point_t::dump(ostream & strm, unsigned int indent) const
{
  strm << INDSCP << "the_supported_point_t(" << (void *)this << ")" << endl
       << INDSCP << "{" << endl;
  the_point_t::dump(strm, INDNXT);
  strm << INDSTR << "ref_ =" << endl;
  ref_->dump(strm, INDNXT);
  strm << INDSTR << "value_ = " << value_ << ";" << endl
       << INDSCP << "}" << endl << endl;
}


//----------------------------------------------------------------
// the_soft_point_t::the_soft_point_t
//
the_soft_point_t::the_soft_point_t():
  the_supported_point_t()
{}

//----------------------------------------------------------------
// the_soft_point_t::the_soft_point_t
//
the_soft_point_t::the_soft_point_t(const the_reference_t & ref):
  the_supported_point_t(ref)
{}

//----------------------------------------------------------------
// the_soft_point_t::the_soft_point_t
//
the_soft_point_t::the_soft_point_t(const the_soft_point_t & point):
  the_supported_point_t(point)
{}

//----------------------------------------------------------------
// the_soft_point_t::dump
//
void
the_soft_point_t::dump(ostream & strm, unsigned int indent) const
{
  strm << INDSCP << "the_soft_point_t(" << (void *)this << ")" << endl
       << INDSCP << "{" << endl;
  the_supported_point_t::dump(strm, INDNXT);
  strm << INDSCP << "}" << endl << endl;
}


//----------------------------------------------------------------
// the_sticky_point_t::the_sticky_point_t
//
the_sticky_point_t::the_sticky_point_t():
  the_supported_point_t()
{}

//----------------------------------------------------------------
// the_sticky_point_t::the_sticky_point_t
//
the_sticky_point_t::the_sticky_point_t(const the_reference_t & ref):
  the_supported_point_t(ref)
{}

//----------------------------------------------------------------
// the_sticky_point_t::the_sticky_point_t
//
the_sticky_point_t::the_sticky_point_t(const the_sticky_point_t & point):
  the_supported_point_t(point)
{}

//----------------------------------------------------------------
// the_sticky_point_t::added_to_the_registry
//
void
the_sticky_point_t::added_to_the_registry(the_registry_t * registry,
                                        const unsigned int & id)
{
  the_point_t::added_to_the_registry(registry, id);

  // update the graph:
  establish_supporter_dependent(registry, ref_->id(), id);

  ref_->eval(registry, value_);
  anchor_ = value_;
}

//----------------------------------------------------------------
// the_sticky_point_t::regenerate
//
bool
the_sticky_point_t::regenerate()
{
  the_registry_t * r = registry();

  // FIXME: re-implement to be generic and independent of
  // supporting geometry reference type:

  the_curve_t * curve = ref_->references<the_curve_t>(r);
  if (curve)
  {
    the_point_curve_deviation_t deviation(value_, curve->geom());
    std::list<the_deviation_min_t> solution;
    if (deviation.find_local_minima(solution))
    {
      solution.sort();

      const the_deviation_min_t & srs = solution.front();
      the_curve_ref_t * crv_ref = dynamic_cast<the_curve_ref_t *>(ref_);
      assert(crv_ref);

      crv_ref->set_param(srs.s_);
    }
  }

  // look up the reference, evaluate the paramater with
  // respect to the reference, return the value:
  return ref_->eval(r, value_);
}

//----------------------------------------------------------------
// the_sticky_point_t::dump
//
void
the_sticky_point_t::dump(ostream & strm, unsigned int indent) const
{
  strm << INDSCP << "the_sticky_point_t(" << (void *)this << ")" << endl
       << INDSCP << "{" << endl;
  the_supported_point_t::dump(strm, INDNXT);
  strm << INDSCP << "}" << endl << endl;
}


//----------------------------------------------------------------
// the_point_ref_t::the_point_ref_t
//
the_point_ref_t::the_point_ref_t(unsigned int id):
  the_reference_t(id)
{}

//----------------------------------------------------------------
// the_point_ref_t::eval
//
bool
the_point_ref_t::eval(the_registry_t * r, p3x1_t & pt) const
{
  the_point_t * vert = r->elem<the_point_t>(id());
  if (vert == NULL) return false;

  pt = vert->value();
  return true;
}

//----------------------------------------------------------------
// the_point_ref_t::dump
//
void
the_point_ref_t::dump(ostream & strm, unsigned int indent) const
{
  strm << INDSCP << "the_point_ref_t(" << (void *)this << ")" << endl
       << INDSCP << "{" << endl;
  the_reference_t::dump(strm, INDNXT);
  strm << INDSCP << "}" << endl << endl;
}
