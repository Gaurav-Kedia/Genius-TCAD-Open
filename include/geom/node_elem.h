// $Id: node_elem.h,v 1.6 2008/05/25 02:42:09 gdiso Exp $

// The libMesh Finite Element Library.
// Copyright (C) 2002-2007  Benjamin S. Kirk, John W. Peterson

// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA



#ifndef __node_elem_h__
#define __node_elem_h__

// C++ includes

// Local includes
#include "elem.h"


// Forward declarations


/**
 * The \p NodeElem is a point element, generally used as
 * a side of a 1D element.
 */

// ------------------------------------------------------------
// NodeElem class definition
class NodeElem : public Elem
{
 public:

  /**
   * Constructor.  By default this element has no parent.
   */
  NodeElem (Elem* p=NULL) :
    Elem(NodeElem::n_nodes(), NodeElem::n_sides(), p) {}

  /**
   * Constructor.  Explicitly specifies the number of
   * nodes and neighbors for which storage will be allocated.
   */
  NodeElem (const unsigned int nn,
         const unsigned int ns,
         Elem* p) :
    Elem(nn, ns, p) { genius_assert(nn == 1); assert (ns == 0); }

  /**
   * Default node element, takes number of nodes and
   * parent. Derived classes implement 'true' elements.
   */
  NodeElem (const unsigned int nn,
    Elem* p) :
    Elem(nn, NodeElem::n_sides(), p) {}

  /**
   * @returns 0, the dimensionality of the object.
   */
  unsigned int dim () const { return 0; }

  /**
   * @returns 1.
   */
  unsigned int n_nodes() const { return 1; }

  /**
   * @returns 0
   */
  unsigned int n_sides() const { return 0; }

  /**
   * @returns 1.  Every NodeElem is a vertex
   */
  unsigned int n_vertices() const { return 1; }

  /**
   * @returns 0.
   */
  unsigned int n_edges() const { return 0; }

  /**
   * @returns 0.
   */
  unsigned int n_faces() const { return 0; }

  /**
   * @returns 1
   */
  unsigned int n_children() const { return 1; }

  /**
   * @returns true iff the specified child is on the
   * specified side
   */
  virtual bool is_child_on_side(const unsigned int , const unsigned int ) const
  { genius_error(); return false; }

  /**
   * @returns an id associated with the \p s side of this element.
   * This should never be important for NodeElems
   */
  unsigned int key (const unsigned int) const
  { return 0; }


  /**
   * @return the ith node on sth side
   */
  unsigned int side_node(unsigned int , unsigned int ) const
  { genius_error(); return 0; }

  /**
   * The \p Elem::nodes_on_side() member makes no sense for nodes.
   */
  void nodes_on_side (const unsigned int i, std::vector<unsigned int> & nodes ) const
  { genius_error(); return; }

  /**
   * The \p Elem::side() member makes no sense for nodes.
   */
  AutoPtr<DofObject> side (const unsigned int) const
  { genius_error(); AutoPtr<DofObject> ap(NULL); return ap; }

  /**
   * The \p Elem::build_side() member makes no sense for nodes.
   */
  AutoPtr<Elem> build_side (const unsigned int, bool) const
  { genius_error(); AutoPtr<Elem> ap(NULL); return ap; }

  /**
   * The \p Elem::build_edge() member makes no sense for nodes.
   */
  AutoPtr<Elem> build_edge (const unsigned int) const
  { genius_error(); AutoPtr<Elem> ap(NULL); return ap; }

  /**
   * find the vertex on a side of element. this element should be active.
   * the hanging point due to refine mismatch is considered.
   */
  virtual void build_side_vertex(std::vector<const Node *> & , unsigned int) const
  { genius_error(); }

  /**
   * @returns 1
   */
  unsigned int n_sub_elem() const { return 1; }

  /**
   * @returns true iff the specified (local) node number is a vertex.
   */
  virtual bool is_vertex(const unsigned int) const { return true; }

  /**
   * @returns true iff the specified (local) node number is an edge.
   */
  virtual bool is_edge(const unsigned int) const { return false; }

  /**
   * @returns true iff the specified (local) node number is a face.
   */
  virtual bool is_face(const unsigned int) const { return false; }

  /**
   * @returns true iff the specified (local) node number is on the
   * specified side
   */
  virtual bool is_node_on_side(const unsigned int,
                   const unsigned int) const { genius_error(); return false; }

  /**
   * @returns true iff the specified (local) node number is on the
   * specified edge (i.e. "returns true" in 1D)
   */
  virtual bool is_node_on_edge(const unsigned int,
                   const unsigned int) const { genius_error(); return false; }

  /**
   * @returns true iff the specified (local) edge number is on the
   * specified side
   */
  virtual bool is_edge_on_side(const unsigned int , const unsigned int ) const
  { genius_error(); return false; }

  /**
   * get the node local index on edge e
   */
  virtual void nodes_on_edge (const unsigned int,
                              std::vector<unsigned int> & ) const
  { genius_error(); }


  /**
   * get the node local index on edge e
   */
  virtual void nodes_on_edge (const unsigned int,
                              std::pair<unsigned int, unsigned int> & ) const
  { genius_error(); }


  /**
   * @return the nearest point on this element to the given point p
   */
  virtual Point nearest_point(const Point &p, Real * dist = 0) const
  {
    if(dist) *dist = (p-this->point(0)).size();
    return this->point(0);
  }

  /*
   * @returns true iff the element map is definitely affine within
   * numerical tolerances
   */
  virtual bool has_affine_map () const { return true; }

  /**
   * @returns \p NODEELEM
   */
  ElemType type()  const { return NODEELEM; }

  /**
   * @returns FIRST
   */
  Order default_order() const { return FIRST; }

  virtual void connectivity(const unsigned int sc,
                const IOPackage iop,
                std::vector<unsigned int>& conn) const;

  virtual void side_order( const IOPackage , std::vector<unsigned int>& ) const  {}

protected:


#ifdef ENABLE_AMR

  /**
   * Matrix used to create the elements children.
   */
  float embedding_matrix (const unsigned int i,
             const unsigned int j,
             const unsigned int k) const
  { return _embedding_matrix[i][j][k]; }

  /**
   * Matrix that computes new nodal locations/solution values
   * from current nodes/solution.
   */
  static const float _embedding_matrix[1][1][1];

  /**
   * Matrix that allows children to inherit boundary conditions.
   */
  unsigned int side_children_matrix (const unsigned int,
                     const unsigned int) const
  { genius_error(); return 0; }

#endif

};





// ------------------------------------------------------------
// NodeElem class member functions

#endif
