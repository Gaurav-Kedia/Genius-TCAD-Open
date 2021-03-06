/********************************************************************************/
/*     888888    888888888   88     888  88888   888      888    88888888       */
/*   8       8   8           8 8     8     8      8        8    8               */
/*  8            8           8  8    8     8      8        8    8               */
/*  8            888888888   8   8   8     8      8        8     8888888        */
/*  8      8888  8           8    8  8     8      8        8            8       */
/*   8       8   8           8     8 8     8      8        8            8       */
/*     888888    888888888  888     88   88888     88888888     88888888        */
/*                                                                              */
/*       A Three-Dimensional General Purpose Semiconductor Simulator.           */
/*                                                                              */
/*                                                                              */
/*  Copyright (C) 2007-2008                                                     */
/*  Cogenda Pte Ltd                                                             */
/*                                                                              */
/*  Please contact Cogenda Pte Ltd for license information                      */
/*                                                                              */
/*  Author: Gong Ding   gdiso@ustc.edu                                          */
/*                                                                              */
/********************************************************************************/

// C++ includes
#include <numeric>

#include "simulation_system.h"
#include "conductor_region.h"
#include "insulator_region.h"
#include "boundary_condition_charge.h"
#include "parallel.h"

using PhysicalUnit::kb;
using PhysicalUnit::e;


///////////////////////////////////////////////////////////////////////
//----------------Function and Jacobian evaluate---------------------//
///////////////////////////////////////////////////////////////////////


/*---------------------------------------------------------------------
 * do pre-process to function for poisson solver
 */
void ChargedContactBC::Poissin_Function_Preprocess(PetscScalar *, Vec f, std::vector<PetscInt> &src_row,
    std::vector<PetscInt> &dst_row, std::vector<PetscInt> &clear_row)
{
  const SimulationRegion * _r1 = bc_regions().first;
  genius_assert(_r1->type() == InsulatorRegion);
  const SimulationRegion * _r2 = bc_regions().second;
  genius_assert(_r2->type() == ElectrodeRegion || _r2->type() == MetalRegion);

  BoundaryCondition::const_node_iterator node_it = nodes_begin();
  BoundaryCondition::const_node_iterator end_it = nodes_end();
  for(; node_it!=end_it; ++node_it )
  {
    const FVM_Node * metal_node = get_region_fvm_node ( ( *node_it ), _r2 );
    clear_row.push_back(metal_node->global_offset());

    // search all the fvm_node which has *node_it as root node, these fvm_nodes have the same location in geometry,
    // but belong to different regions in logic.
    BoundaryCondition::region_node_iterator  rnode_it     = region_node_begin(*node_it);
    BoundaryCondition::region_node_iterator  end_rnode_it = region_node_end(*node_it);
    for(unsigned int i=0 ; rnode_it!=end_rnode_it; ++i, ++rnode_it  )
    {
      const SimulationRegion * region = (*rnode_it).second.first;
      if( region->type() != InsulatorRegion ) continue;

      const FVM_Node * insulator_node  = (*rnode_it).second.second;
      clear_row.push_back(insulator_node->global_offset());
    }
  }

}


/*---------------------------------------------------------------------
 * build function and its jacobian for poisson solver
 */
void ChargedContactBC::Poissin_Function(PetscScalar * x, Vec f, InsertMode &add_value_flag)
{

  // charged float metal is processed here


  // note, we will use ADD_VALUES to set values of vec f
  // if the previous operator is not ADD_VALUES, we should assembly the vec
  if( (add_value_flag != ADD_VALUES) && (add_value_flag != NOT_SET_VALUES) )
  {
    VecAssemblyBegin(f);
    VecAssemblyEnd(f);
  }

  // for 2D mesh, z_width() is the device dimension in Z direction; for 3D mesh, z_width() is 1.0
  PetscScalar z_width = this->z_width();

  // buffer for float metal surface potential equation \integral {\eps \frac{\partial P}{\partial n} } + \sigma = 0
  std::vector<PetscScalar> surface_equ;

  const SimulationRegion * _r1 = bc_regions().first;
  genius_assert(_r1->type() == InsulatorRegion);
  const SimulationRegion * _r2 = bc_regions().second;
  genius_assert(_r2->type() == ElectrodeRegion || _r2->type() == MetalRegion);

  // the float metal psi in current iteration
  genius_assert( this->inter_connect_hub()->local_offset() != invalid_uint );
  PetscScalar phi_f = x[this->inter_connect_hub()->local_offset()];



  // search for all the node with this boundary type
  BoundaryCondition::const_node_iterator node_it = nodes_begin();
  BoundaryCondition::const_node_iterator end_it = nodes_end();
  for(; node_it!=end_it; ++node_it )
  {

    const FVM_Node * metal_node = get_region_fvm_node ( ( *node_it ), _r2 );
    const FVM_NodeData * metal_node_data = metal_node->node_data();

    // psi of metal node
    PetscScalar V_metal = x[metal_node->local_offset()];

    // psi of metal node phi + affinity = fermi
    PetscScalar f_fermi = V_metal + metal_node_data->affinity()/e - phi_f;
    VecSetValue(f,  metal_node->global_offset(), f_fermi, ADD_VALUES);

    // search all the fvm_node which has *node_it as root node, these fvm_nodes have the same location in geometry,
    // but belong to different regions in logic.
    BoundaryCondition::region_node_iterator  rnode_it     = region_node_begin(*node_it);
    BoundaryCondition::region_node_iterator  end_rnode_it = region_node_end(*node_it);
    for(unsigned int i=0 ; rnode_it!=end_rnode_it; ++i, ++rnode_it  )
    {
      const SimulationRegion * region = (*rnode_it).second.first;
      if( region->type() != InsulatorRegion ) continue;

      const FVM_Node * insulator_node  = (*rnode_it).second.second;
      const FVM_NodeData * insulator_node_data = insulator_node->node_data();

      // psi of insulator node
      PetscScalar V_insulator = x[insulator_node->local_offset()];

      // the psi of insulator node is equal to psi of float metal
      PetscScalar f_psi = V_insulator - V_metal;
      VecSetValue(f,  insulator_node->global_offset(), f_psi, ADD_VALUES);

      // process float metal surface equation
      {
        FVM_Node::fvm_neighbor_node_iterator nb_it = insulator_node->neighbor_node_begin();
        FVM_Node::fvm_neighbor_node_iterator nb_it_end = insulator_node->neighbor_node_end();
        for(; nb_it != nb_it_end; ++nb_it)
        {
          const FVM_Node *nb_node = (*nb_it).first;
          // the psi of neighbor node
          PetscScalar V_nb = x[nb_node->local_offset()];
          // distance from nb node to this node
          PetscScalar distance = insulator_node->distance(nb_node);
          // area of out surface of control volume related with neighbor node,
          // here we should consider the difference of 2D/3D by multiply z_width
          PetscScalar cv_boundary = insulator_node->cv_surface_area(nb_node)*z_width;
          // surface electric field
          PetscScalar E = (V_nb-V_insulator)/distance;
          PetscScalar D = insulator_node_data->eps()*E;
          // insert surface integral of electric displacement of this node into surface_equ buffer
          surface_equ.push_back(cv_boundary*D);
        }
      }
    }

  }




  // we sum all the terms in surface_equ buffer, that is surface integral of electric displacement for this boundary
  PetscScalar surface_integral_electric_displacement = std::accumulate(surface_equ.begin(), surface_equ.end(), 0.0 );

  // add to ChargeIntegralBC
  VecSetValue(f, this->inter_connect_hub()->global_offset(), surface_integral_electric_displacement, ADD_VALUES);

  add_value_flag = ADD_VALUES;

}








/*---------------------------------------------------------------------
 * do pre-process to jacobian matrix for poisson solver
 */
void ChargedContactBC::Poissin_Jacobian_Preprocess(PetscScalar *, SparseMatrix<PetscScalar> *jac, std::vector<PetscInt> &src_row,
    std::vector<PetscInt> &dst_row, std::vector<PetscInt> &clear_row)
{

  const SimulationRegion * _r1 = bc_regions().first;
  genius_assert(_r1->type() == InsulatorRegion);
  const SimulationRegion * _r2 = bc_regions().second;
  genius_assert(_r2->type() == ElectrodeRegion || _r2->type() == MetalRegion);

  // search for all the node with this boundary type
  BoundaryCondition::const_node_iterator node_it = nodes_begin();
  BoundaryCondition::const_node_iterator end_it = nodes_end();
  for(; node_it!=end_it; ++node_it )
  {
    const FVM_Node * metal_node = get_region_fvm_node ( ( *node_it ), _r2 );
    clear_row.push_back(metal_node->global_offset());

    // search all the fvm_node which has *node_it as root node, these fvm_nodes have the same location in geometry,
    // but belong to different regions in logic.
    BoundaryCondition::region_node_iterator  rnode_it     = region_node_begin(*node_it);
    BoundaryCondition::region_node_iterator  end_rnode_it = region_node_end(*node_it);
    for(unsigned int i=0 ; rnode_it!=end_rnode_it; ++i, ++rnode_it  )
    {
      const SimulationRegion * region = (*rnode_it).second.first;
      if( region->type() != InsulatorRegion ) continue;

      const FVM_Node * insulator_node  = (*rnode_it).second.second;
      clear_row.push_back(insulator_node->global_offset());
    }
  }

}



/*---------------------------------------------------------------------
 * build function and its jacobian for poisson solver
 */
void ChargedContactBC::Poissin_Jacobian(PetscScalar * x, SparseMatrix<PetscScalar> *jac, InsertMode &add_value_flag)
{
  // Jacobian of Electrode-Insulator interface is processed here

  const SimulationRegion * _r1 = bc_regions().first;
  genius_assert(_r1->type() == InsulatorRegion);
  const SimulationRegion * _r2 = bc_regions().second;
  genius_assert(_r2->type() == ElectrodeRegion || _r2->type() == MetalRegion);

  //the indepedent variable number, we need 3 here.
  adtl::AutoDScalar::numdir=3;

  // the float metal psi in current iteration
  genius_assert( this->inter_connect_hub()->local_offset() != invalid_uint );
  AutoDScalar phi_f = x[this->inter_connect_hub()->local_offset()]; phi_f.setADValue(0, 1.0);

  // for 2D mesh, z_width() is the device dimension in Z direction; for 3D mesh, z_width() is 1.0
  PetscScalar z_width = this->z_width();

  // after that, set new Jacobian entrance to source rows
  BoundaryCondition::const_node_iterator node_it = nodes_begin();
  BoundaryCondition::const_node_iterator end_it = nodes_end();
  for(; node_it!=end_it; ++node_it )
  {
    genius_assert((*node_it)->on_processor());
    const FVM_Node * metal_node = get_region_fvm_node ( ( *node_it ), _r2 );
    const FVM_NodeData * metal_node_data = metal_node->node_data();

    // psi of metal node
    AutoDScalar V_metal = x[metal_node->local_offset()];  V_metal.setADValue(1, 1.0);

    // psi of metal node phi + affinity = fermi
    AutoDScalar f_fermi = V_metal + metal_node_data->affinity()/e - phi_f;
    // set Jacobian of governing equation ff
    jac->add(metal_node->global_offset(), metal_node->global_offset(), f_fermi.getADValue(1));
    jac->add(metal_node->global_offset(), this->inter_connect_hub()->global_offset(), f_fermi.getADValue(0));


    // search all the fvm_node which has *node_it as root node, these fvm_nodes have the same location in geometry,
    // but belong to different regions in logic.
    BoundaryCondition::region_node_iterator  rnode_it     = region_node_begin(*node_it);
    BoundaryCondition::region_node_iterator  end_rnode_it = region_node_end(*node_it);
    for(unsigned int i=0 ; rnode_it!=end_rnode_it; ++i, ++rnode_it  )
    {
      const SimulationRegion * region = (*rnode_it).second.first;
      if( region->type() != InsulatorRegion ) continue;

      const FVM_Node * insulator_node  = (*rnode_it).second.second;
      const FVM_NodeData * insulator_node_data = insulator_node->node_data();
      // psi of insulator node
      AutoDScalar V_insulator = x[insulator_node->local_offset()];  V_insulator.setADValue(2, 1.0);

      // the psi of insulator node is equal to psi of float metal
      AutoDScalar f_psi = V_insulator - V_metal;
      // set Jacobian of governing equation f_psi
      jac->add(insulator_node->global_offset(), insulator_node->global_offset(), f_psi.getADValue(2));
      jac->add(insulator_node->global_offset(), metal_node->global_offset(), f_psi.getADValue(1));

      FVM_Node::fvm_neighbor_node_iterator nb_it = insulator_node->neighbor_node_begin();
      FVM_Node::fvm_neighbor_node_iterator nb_it_end = insulator_node->neighbor_node_end();
      for(; nb_it != nb_it_end; ++nb_it)
      {
        const FVM_Node *nb_node = (*nb_it).first;
        genius_assert(nb_node->on_local());

        // the psi of neighbor node
        AutoDScalar V_nb = x[nb_node->local_offset()];  V_nb.setADValue(0, 1.0);
        // distance from nb node to this node
        PetscScalar distance = insulator_node->distance(nb_node);
        // area of out surface of control volume related with neighbor node,
        // here we should consider the difference of 2D/3D by multiply z_width
        PetscScalar cv_boundary = insulator_node->cv_surface_area(nb_node)*z_width;
        // surface electric field and electrc displacement
        AutoDScalar E = (V_nb-V_insulator)/distance;
        AutoDScalar D = insulator_node_data->eps()*E;

        // since the governing equation is \sum \integral D + \sigma = 0
        // and \integral D = \sum cv_boundary*D
        // here we use MatSetValue ADD_VALUES to process \sum \integral D one by one
        jac->add(this->inter_connect_hub()->global_offset(), insulator_node->global_offset(), (cv_boundary*D).getADValue(2));
        jac->add(this->inter_connect_hub()->global_offset(), nb_node->global_offset(), (cv_boundary*D).getADValue(0));
      }
    }
  }

  // the last operator is ADD_VALUES
  add_value_flag = ADD_VALUES;

}


/*---------------------------------------------------------------------
 * update potential of float metal
 */
void ChargedContactBC::Poissin_Update_Solution(PetscScalar *lxx)
{
  const SimulationRegion * _r2 = bc_regions().second;
  this->psi() = lxx[this->inter_connect_hub()->local_offset()] - _r2->get_affinity()/e;
}

