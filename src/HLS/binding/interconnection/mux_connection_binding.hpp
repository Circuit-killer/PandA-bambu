/*
 *
 *                   _/_/_/    _/_/   _/    _/ _/_/_/    _/_/
 *                  _/   _/ _/    _/ _/_/  _/ _/   _/ _/    _/
 *                 _/_/_/  _/_/_/_/ _/  _/_/ _/   _/ _/_/_/_/
 *                _/      _/    _/ _/    _/ _/   _/ _/    _/
 *               _/      _/    _/ _/    _/ _/_/_/  _/    _/
 *
 *             ***********************************************
 *                              PandA Project
 *                     URL: http://panda.dei.polimi.it
 *                       Politecnico di Milano - DEIB
 *                        System Architectures Group
 *             ***********************************************
 *              Copyright (c) 2004-2018 Politecnico di Milano
 *
 *   This file is part of the PandA framework.
 *
 *   The PandA framework is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
*/
/**
 * @file mux_connection_binding.hpp
 * @brief Class to manage mux-based interconnections based on the FSM controller
 *
 * @author Christian Pilato <pilato@elet.polimi.it>
 * @author Fabrizio Ferrandi <fabrizio.ferrandi@polimi.it>
 * $Revision$
 * $Date$
 * Last modified by $Author$
 *
*/

#ifndef MUX_CONNECTION_BINDING_HPP
#define MUX_CONNECTION_BINDING_HPP

#include "conn_binding_creator.hpp"
REF_FORWARD_DECL(graph);
class array_ref;

#include "conn_binding.hpp"

#include "hls_manager.hpp"

#include <unordered_map>

REF_FORWARD_DECL(conn_binding);
CONSTREF_FORWARD_DECL(OpGraph);

/**
 * Definition of hash function for EdgeDescriptor
 */
namespace std
{
   template <>
      struct hash<std::tuple<generic_objRef, unsigned int> > : public unary_function<std::tuple<generic_objRef, unsigned int>, size_t>
      {
         size_t operator()(std::tuple<generic_objRef, unsigned int> p) const
         {
            size_t hash_value = 0;
            boost::hash_combine(hash_value, std::get<0>(p));
            boost::hash_combine(hash_value, std::get<1>(p));

            return hash_value;
         }
      };
}

/**
 * @class mux_connection_binding
 * @ingroup interconnect
 * @brief Class managing the connection binding of datapath. It extends the standard mux_connection_binding class.
 *
 * This class provides methods to compute interconnections among datapath elements (functional units, registers,
 * ports). Different architectural style have been implemented: mux-based or bus-based. They are implemented as
 * specialization of generic interconnections.
 *
 */
class mux_connection_binding : public conn_binding_creator
{
   private:

      /// type representing a resource identifier
      typedef std::pair<unsigned int, unsigned int> resource_id_type;

      /// store the registers for each resource and for each port
      std::map<resource_id_type, std::map<unsigned int, std::set<unsigned int> > > regs_in;
      /// store the chained storage values for each resource and for each port
      std::map<resource_id_type, std::map<unsigned int, std::set<unsigned int> > > chained_in;
      /// store the resource in IN for each resource and for each port
      std::map<resource_id_type, std::map<unsigned int, std::set<resource_id_type > > > module_in;

      /// store the operations for which a port swapping is beneficial
      std::set<vertex> swap_computed_table;

      /// store the operations for which a port swapping is not beneficial
      std::set<vertex> noswap_computed_table;

      /// variable used to assign a unique id to sparse logic
      unsigned int id;

      /// store the current phi use. Used during the analysis of phi nodes
      unsigned int cur_phi_tree_var;

      ///Last computed mux interconnection
     conn_bindingRef last_conn_binding;

     bool is_PC;

      /**
       * Performs specialization of interconnections using mux architecture.
       */
      unsigned int mux_interconnection();

      /**
       * Computes logic for inputs. The connection can become directed or by multiplexer. If multiplexers are
       * allocated, then decoding logic is created.
       * @param src is the set of references to generic_obj, sources of connections
       * @param tgt is the reference to connection target
       * @param op is i-th operand of target element, where source is attached
       * @param port_index is the i-th port index of the given target port.
       * @return number of multiplexer allocated
       */
      unsigned int input_logic(const conn_binding::ConnectionSources & src, const generic_objRef tgt, unsigned int op, unsigned int port_index, unsigned int iteration);

      /**
       * Determine the actual interconnection
       */
      void determine_connection(const vertex& op, const HLS_manager::io_binding_type& var, generic_objRef fu_obj, unsigned int port_num, unsigned int port_index, const OpGraphConstRef data, unsigned int precision, unsigned int alignment=0);

      /**
       * Compute the bitsize given a io_binding type
       */
      unsigned int object_bitsize(const tree_managerRef TreeM, const HLS_manager::io_binding_type & obj) const;

      /**
      * Recursive function which returns the offset of a dynamic multidimensional array call
      */
      generic_objRef dynamic_multidimensional_array_handler(array_ref * ar, const vertex& op, const OpGraphConstRef data, unsigned int &base_address_index_pointer, std::vector<unsigned int> &recursive_indexes_values, std::vector<unsigned int> &dims, generic_objRef &global_adder, const bool is_not_a_phi);

      /**
       * @brief connect_array_index: connect the index port of an array_ref and convert it in case the source is of int type
       */
      void
      connect_array_index(unsigned int tree_index, generic_objRef fu_obj, unsigned int port_num, unsigned int port_index, unsigned int bus_addr_bitsize, const OpGraphConstRef data, const vertex &op);


   private:

      /**
       *  create the connection object and update the unique table
       */
      void create_single_conn(const OpGraphConstRef data, const vertex& op, generic_objRef fu_obj_src,
                                            generic_objRef fu_obj, unsigned int port_num, unsigned int port_index, unsigned int tree_var, unsigned int precision, const bool is_not_a_phi);

      /**
       * connect the fu_obj with the associated registers.
       */
      void connect_to_registers(vertex op, const OpGraphConstRef data, generic_objRef fu_obj, unsigned int port_num, unsigned int port_index, unsigned int tree_var, unsigned int precision, const bool is_not_a_phi);

      unsigned int extract_parm_decl(unsigned int tree_var, const tree_managerRef TreeM);

      void add_conversion(unsigned int num, unsigned int size_tree_var, VertexIterator op, unsigned int form_par_type, unsigned int port_index, const generic_objRef fu_obj, const OpGraphConstRef data, const tree_managerRef TreeM, unsigned int tree_var, std::vector<HLS_manager::io_binding_type> &var_read, unsigned int size_form_par);

      unsigned int address_precision(unsigned int precision, const vertex& op, const OpGraphConstRef data, const tree_managerRef TreeM);

   public:

      /**
       * Main constructor
       * @param design_flow_manager is the design flow manager
       */
      mux_connection_binding(const ParameterConstRef Param, const HLS_managerRef HLSMgr, unsigned int funId, const DesignFlowManagerConstRef design_flow_manager);

      /**
       * Destructor
       */
      virtual ~mux_connection_binding();

      /**
       * Execute the step
       * @return the exit status of this step
       */
      virtual DesignFlowStep_Status InternalExec();

      /**
       * Initialize the step (i.e., like a constructor, but executed just before exec
       */
      virtual void Initialize();

      /**
       * Creates the connections inside the architecture
       */
      void create_connections();

      /**
       * check if the port has to be swapped
      */
      unsigned int swap_p(const OpGraphConstRef data, vertex op, unsigned int num, std::vector<HLS_manager::io_binding_type> &vars_read, const BehavioralHelperConstRef behavioral_helper, const tree_managerRef TreeM);

};

#endif
