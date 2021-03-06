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
 * @file memory_allocation.cpp
 * @brief Base class to allocate memories in high-level synthesis
 *
 * @author Fabrizio Ferrandi <fabrizio.ferrandi@polimi.it>
 * @author Christian Pilato <pilato@elet.polimi.it>
 * $Revision$
 * $Date$
 * Last modified by $Author$
 *
*/
#include "memory_allocation.hpp"

#include "hls_manager.hpp"
#include "hls_target.hpp"
#include "target_device.hpp"
#include "memory.hpp"

#include "behavioral_helper.hpp"
#include "call_graph.hpp"
#include "call_graph_manager.hpp"
#include "function_behavior.hpp"
#include "op_graph.hpp"

#include "polixml.hpp"
#include "xml_helper.hpp"

#include "Parameter.hpp"

#include "math_function.hpp"

///tree includes
#include "ext_tree_node.hpp"
#include "tree_helper.hpp"
#include "tree_manager.hpp"
#include "tree_node.hpp"
#include "tree_reindex.hpp"

MemoryAllocationSpecialization::MemoryAllocationSpecialization(const MemoryAllocation_Policy _memory_allocation_policy, const MemoryAllocation_ChannelsType _memory_allocation_channels_type) :
   memory_allocation_policy(_memory_allocation_policy),
   memory_allocation_channels_type(_memory_allocation_channels_type)
{
}

const std::string MemoryAllocationSpecialization::GetKindText() const
{
   std::string ret;
   switch(memory_allocation_policy)
   {
      case MemoryAllocation_Policy::LSS:
         ret += "LSS";
         break;
      case MemoryAllocation_Policy::GSS:
         ret += "GSS";
         break;
      case MemoryAllocation_Policy::ALL_BRAM:
         ret += "ALL_BRAM";
         break;
      case MemoryAllocation_Policy::NO_BRAM:
         ret += "NO_BRAM";
         break;
      case MemoryAllocation_Policy::EXT_PIPELINED_BRAM:
         ret += "EXT_PIPELINED_BRAM";
         break;
      case MemoryAllocation_Policy::NONE:
      default:
         THROW_UNREACHABLE("");
   }
   ret += "-";
   switch(memory_allocation_channels_type)
   {
      case MemoryAllocation_ChannelsType::MEM_ACC_11:
         ret += "11";
         break;
      case MemoryAllocation_ChannelsType::MEM_ACC_N1:
         ret += "N1";
         break;
      case MemoryAllocation_ChannelsType::MEM_ACC_NN:
         ret += "NN";
         break;
      case MemoryAllocation_ChannelsType::MEM_ACC_P1N:
         ret += "P1N";
         break;
      default:
         THROW_UNREACHABLE("");
   }
   return ret;
}

const std::string MemoryAllocationSpecialization::GetSignature() const
{
   return STR(static_cast<unsigned int>(memory_allocation_policy)) + "::" + STR(static_cast<unsigned int>(memory_allocation_channels_type));
}

memory_allocation::memory_allocation(const ParameterConstRef _parameters, const HLS_managerRef _HLSMgr, const DesignFlowManagerConstRef _design_flow_manager, const HLSFlowStep_Type _hls_flow_step_type, const HLSFlowStepSpecializationConstRef _hls_flow_step_specialization) :
   HLS_step(_parameters, _HLSMgr, _design_flow_manager, _hls_flow_step_type, _hls_flow_step_specialization and GetPointer<const MemoryAllocationSpecialization>(_hls_flow_step_specialization) ? _hls_flow_step_specialization : HLSFlowStepSpecializationConstRef(new MemoryAllocationSpecialization(_parameters->getOption<MemoryAllocation_Policy>(OPT_memory_allocation_policy), _parameters->getOption<MemoryAllocation_ChannelsType>(OPT_channels_type)))),
   already_executed(false),
   ///NOTE: hls_flow_step_specialization and not _hls_flow_step_specialization is correct
   memory_allocation_policy(GetPointer<const MemoryAllocationSpecialization>(hls_flow_step_specialization)->memory_allocation_policy)
{
   debug_level = parameters->get_class_debug_level(GET_CLASS(*this));
}

memory_allocation::~memory_allocation()
{

}

const std::unordered_set<std::tuple<HLSFlowStep_Type, HLSFlowStepSpecializationConstRef, HLSFlowStep_Relationship> > memory_allocation::ComputeHLSRelationships(const DesignFlowStep::RelationshipType relationship_type) const
{
   std::unordered_set<std::tuple<HLSFlowStep_Type, HLSFlowStepSpecializationConstRef, HLSFlowStep_Relationship> > ret;
   switch(relationship_type)
   {
      case DEPENDENCE_RELATIONSHIP:
         {
            ret.insert(std::make_tuple(parameters->getOption<HLSFlowStep_Type>(OPT_function_allocation_algorithm), HLSFlowStepSpecializationConstRef(), HLSFlowStep_Relationship::WHOLE_APPLICATION));
            break;
         }
      case INVALIDATION_RELATIONSHIP:
         {
            break;
         }
      case PRECEDENCE_RELATIONSHIP:
         {
            break;
         }
      default:
         THROW_UNREACHABLE("");
   }
   return ret;
}

void memory_allocation::setup_memory_allocation()
{
   const auto cg_man = HLSMgr->CGetCallGraphManager();
   func_list = cg_man->GetReachedBodyFunctions();
   /// the analysis has to be performed only on the reachable functions
   for (const auto It : func_list)
   {
      const FunctionBehaviorConstRef function_behavior = HLSMgr->CGetFunctionBehavior(It);
      /// add parm_decls that have to be copied
      const std::set<unsigned int>& parm_decl_copied = function_behavior->get_parm_decl_copied();
      for(std::set<unsigned int>::const_iterator p = parm_decl_copied.begin(); p != parm_decl_copied.end(); ++p)
      {
         HLSMgr->Rmem->add_parm_decl_copied(*p);
      }
      /// add parm_decls that have to be stored
      const std::set<unsigned int>& parm_decl_stored = function_behavior->get_parm_decl_stored();
      for(std::set<unsigned int>::const_iterator p = parm_decl_stored.begin(); p != parm_decl_stored.end(); ++p)
      {
         HLSMgr->Rmem->add_parm_decl_stored(*p);
      }
      /// add actual parameters that have to be loaded
      const std::set<unsigned int>& parm_decl_loaded = function_behavior->get_parm_decl_loaded();
      for(std::set<unsigned int>::const_iterator p = parm_decl_loaded.begin(); p != parm_decl_loaded.end(); ++p)
      {
         HLSMgr->Rmem->add_actual_parm_loaded(*p);
      }
   }
}

void memory_allocation::finalize_memory_allocation()
{
   THROW_ASSERT(func_list.size(), "Empty list of functions to be analyzed");
   bool use_unknown_address = false;
   bool pointer_conversion_happen= false;
   bool has_unaligned_accesses = false;
   bool assume_aligned_access_p = parameters->isOption(OPT_aligned_access) && parameters->getOption<bool>(OPT_aligned_access);
   const tree_managerRef TreeM = HLSMgr->get_tree_manager();
   for(const auto It : func_list)
   {
      const FunctionBehaviorConstRef FB = HLSMgr->CGetFunctionBehavior(It);
      const BehavioralHelperConstRef BH = FB->CGetBehavioralHelper();
      const std::list<unsigned int>& function_parameters = BH->get_parameters();

      if(FB->get_dereference_unknown_addr())
         INDENT_OUT_MEX(OUTPUT_LEVEL_VERBOSE, output_level, "---This function uses unknown addresses deref: " + BH->get_function_name());

      use_unknown_address |= FB->get_dereference_unknown_addr();

      if(FB->get_pointer_type_conversion())
         INDENT_OUT_MEX(OUTPUT_LEVEL_VERBOSE, output_level, "---This function performs pointer conversions: " + BH->get_function_name());

      pointer_conversion_happen |= FB->get_pointer_type_conversion();

      if(FB->get_unaligned_accesses())
      {
         INDENT_OUT_MEX(OUTPUT_LEVEL_VERBOSE, output_level, "---This function performs unaligned accesses: " + BH->get_function_name());
         if(assume_aligned_access_p)
            THROW_ERROR("Option --aligned-access have been specified on a function with unaligned accesses");
      }

      has_unaligned_accesses |= FB->get_unaligned_accesses();

      for(auto const parameter : function_parameters)
      {
         if(HLSMgr->Rmem->is_parm_decl_copied(parameter) && !HLSMgr->Rmem->is_parm_decl_stored(parameter))
         {
            HLSMgr->Rmem->set_implicit_memcpy(true);
         }
      }
   }

   if(HLSMgr->Rmem->has_implicit_memcpy())
   {
      unsigned int memcpy_function_id = TreeM->function_index("__builtin_memcpy");
      func_list.insert(memcpy_function_id);
   }

   unsigned int maximum_bus_size = 0;
   bool use_databus_width = false;
   bool has_intern_shared_data = false;
   bool has_misaligned_indirect_ref = HLSMgr->Rmem->has_packed_vars();
   bool needMemoryMappedRegisters=false;
   const CallGraphManagerConstRef call_graph_manager = HLSMgr->CGetCallGraphManager();
   /// looking for the maximum data bus size needed
   for(auto fun_id : func_list)
   {
      const FunctionBehaviorConstRef function_behavior = HLSMgr->CGetFunctionBehavior(fun_id);
      const BehavioralHelperConstRef behavioral_helper = function_behavior->CGetBehavioralHelper();
      bool is_interfaced = HLSMgr->hasToBeInterfaced(behavioral_helper->get_function_index());
      if(function_behavior->get_has_globals() && (!parameters->isOption(OPT_do_not_expose_globals) || !parameters->getOption<bool>(OPT_do_not_expose_globals)))
         has_intern_shared_data = true;
      const std::list<unsigned int>& function_parameters = behavioral_helper->get_parameters();
      for(const auto function_parameter : function_parameters)
      {
         if(HLSMgr->Rmem->is_parm_decl_copied(function_parameter) && !HLSMgr->Rmem->is_parm_decl_stored(function_parameter))
         {
            use_databus_width = true;
            maximum_bus_size = std::max(maximum_bus_size, 8u);
         }
         if(!use_unknown_address && is_interfaced && tree_helper::is_a_pointer(TreeM, function_parameter))
         {
            use_unknown_address = true;
            THROW_WARNING("This function uses unknown addresses: " + behavioral_helper->get_function_name());
         }
      }
      if(function_behavior->has_packed_vars())
         has_misaligned_indirect_ref = true;
      const std::set<unsigned int>& parm_decl_stored = function_behavior->get_parm_decl_stored();
      for(std::set<unsigned int>::const_iterator p = parm_decl_stored.begin(); p != parm_decl_stored.end(); ++p)
      {
         maximum_bus_size = std::max(maximum_bus_size, tree_helper::size(TreeM, tree_helper::get_type_index(TreeM,*p)));
      }
      PRINT_DBG_MEX(DEBUG_LEVEL_VERBOSE, debug_level, "Analyzing function for bus size: " + behavioral_helper->get_function_name());
      const OpGraphConstRef g = function_behavior->CGetOpGraph(FunctionBehavior::CFG);
      graph::vertex_iterator v, v_end;
      for (boost::tie(v, v_end) = boost::vertices(*g); v != v_end; ++v)
      {
         std::string current_op = g->CGetOpNodeInfo(*v)->GetOperation();
         std::vector<HLS_manager::io_binding_type> var_read = HLSMgr->get_required_values(fun_id, *v);
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->Analyzing operation " + GET_NAME(g, *v));
         if(GET_TYPE(g, *v) & (TYPE_LOAD | TYPE_STORE))
         {
            const tree_nodeRef curr_tn = TreeM->get_tree_node_const(g->CGetOpNodeInfo(*v)->GetNodeId());
            gimple_assign * me = GetPointer<gimple_assign>(curr_tn);
            THROW_ASSERT(me, "only gimple_assign's are allowed as memory operations");
            unsigned int var = 0;
            unsigned int expr_index;
            if(GET_TYPE(g, *v) | TYPE_STORE)
               expr_index = GET_INDEX_NODE(me->op0);
            else
               expr_index = GET_INDEX_NODE(me->op1);
            var = tree_helper::get_base_index(TreeM, expr_index);
            if(tree_helper::is_a_misaligned_vector(TreeM, expr_index))
               has_misaligned_indirect_ref = true;
            /// check for packed struct/union accesses
            if(!has_misaligned_indirect_ref)
               has_misaligned_indirect_ref = tree_helper::is_packed_access(TreeM,expr_index);

            /// check if a global variable may be accessed from an external component
            if(!has_intern_shared_data && var && function_behavior->is_variable_mem(var) && !HLSMgr->Rmem->is_private_memory(var) && (!parameters->isOption(OPT_do_not_expose_globals) || !parameters->getOption<bool>(OPT_do_not_expose_globals)))
            {
               const tree_nodeRef var_tn = TreeM->get_tree_node_const(var);
               var_decl *vd = GetPointer<var_decl>(var_tn);
               if(vd &&
                  (((!vd->scpe || GET_NODE(vd->scpe)->get_kind() == translation_unit_decl_K) && !vd->static_flag) ||
                   tree_helper::is_volatile(TreeM,var) ||
                   call_graph_manager->ExistsAddressedFunction()))
                  has_intern_shared_data = true; /// an external component can access the var possibly (global and volative vars)
            }
            unsigned int value_bitsize;
            if (GET_TYPE(g, *v) & TYPE_STORE)
            {
               unsigned int size_var = std::get<0>(var_read[0]);
               unsigned int size_type_index = tree_helper::get_type_index(TreeM, size_var);
               value_bitsize = tree_helper::size(TreeM, size_type_index);
            }
            else
            {
               unsigned int size_var = HLSMgr->get_produced_value(fun_id, *v);
               unsigned int size_type_index = tree_helper::get_type_index(TreeM, size_var);
               value_bitsize = tree_helper::size(TreeM, size_type_index);
            }
            if(!(function_behavior->is_variable_mem(var) && HLSMgr->Rmem->is_private_memory(var)))
               maximum_bus_size = std::max(maximum_bus_size, value_bitsize);
            PRINT_DBG_MEX(DEBUG_LEVEL_VERBOSE, debug_level, " with maximum_bus_size=" + STR(maximum_bus_size));
         }
         else
         {
            if(current_op == MEMCPY || current_op == MEMCMP || current_op == MEMSET)
            {
               use_databus_width = true;
               maximum_bus_size = std::max(maximum_bus_size, 8u);
               if(assume_aligned_access_p)
                  THROW_ERROR("Option --aligned-access cannot be used in presence of memcpy, memcmp or memset");
            }
            std::vector<HLS_manager::io_binding_type>::const_iterator vr_it_end = var_read.end();
            for(std::vector<HLS_manager::io_binding_type>::const_iterator vr_it = var_read.begin(); vr_it != vr_it_end; ++vr_it)
            {
               unsigned int var = std::get<0>(*vr_it);
               if(var && tree_helper::is_a_pointer(TreeM, var))
               {
                  unsigned int type_index;
                  const tree_nodeRef var_node = TreeM->get_tree_node_const(var);
                  const tree_nodeRef type_node = tree_helper::get_type_node(var_node, type_index);
                  tree_nodeRef type_node_ptd;
                  if(type_node->get_kind() == pointer_type_K)
                     type_node_ptd = GetPointer<pointer_type>(type_node)->ptd;
                  else if(type_node->get_kind() == reference_type_K)
                     type_node_ptd = GetPointer<reference_type>(type_node)->refd;
                  else
                     THROW_ERROR("A pointer type is expected");
                  unsigned bitsize = 1;
                  tree_helper::accessed_greatest_bitsize(TreeM, GET_NODE(type_node_ptd), GET_INDEX_NODE(type_node_ptd), bitsize);
                  maximum_bus_size = std::max(maximum_bus_size, bitsize);
               }
            }
         }
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Analyzed " + GET_NAME(g, *v));
      }
      const auto top_functions = HLSMgr->CGetCallGraphManager()->GetRootFunctions();
      bool local_needMemoryMappedRegisters =
         (top_functions.find(fun_id) != top_functions.end() &&
          parameters->getOption<HLSFlowStep_Type>(OPT_interface_type) == HLSFlowStep_Type::WB4_INTERFACE_GENERATION) ||
         (HLSMgr->hasToBeInterfaced(fun_id) &&
          top_functions.find(fun_id) == top_functions.end()) ||
           parameters->getOption<bool>(OPT_memory_mapped_top);
      needMemoryMappedRegisters = needMemoryMappedRegisters || local_needMemoryMappedRegisters;
      if(local_needMemoryMappedRegisters)
      {
         unsigned int addr_bus_bitsize;
         if(parameters->isOption(OPT_addr_bus_bitsize))
            addr_bus_bitsize = parameters->getOption<unsigned int>(OPT_addr_bus_bitsize);
         else
            addr_bus_bitsize = 32;
         for (auto par : behavioral_helper->get_parameters())
         {
            unsigned int type_index = tree_helper::get_type_index(TreeM, par);
            bool is_a_struct_union = tree_helper::is_a_struct(TreeM, type_index) || tree_helper::is_an_union(TreeM, type_index) || tree_helper::is_a_complex(TreeM, type_index);
            if(is_a_struct_union)
               maximum_bus_size = std::max(addr_bus_bitsize,maximum_bus_size);
            else
               maximum_bus_size = std::max(tree_helper::size(TreeM,  par),maximum_bus_size);
         }
         const unsigned int function_return = behavioral_helper->GetFunctionReturnType(fun_id);
         if(function_return)
         {
            maximum_bus_size = std::max(tree_helper::size(TreeM,  function_return), maximum_bus_size);
         }
      }
   }

   const HLS_targetRef HLS_T = HLSMgr->get_HLS_target();
   unsigned int aligned_bitsize;
   unsigned int bram_bitsize, data_bus_bitsize, addr_bus_bitsize, size_bus_bitsize;
   unsigned int bram_bitsize_min = HLS_T->get_target_device()->get_parameter<unsigned int>("BRAM_bitsize_min");
   unsigned int bram_bitsize_max = HLS_T->get_target_device()->get_parameter<unsigned int>("BRAM_bitsize_max");
   HLSMgr->Rmem->set_maxbram_bitsize(bram_bitsize_max);

   maximum_bus_size = resize_to_1_8_16_32_64_128_256_512(maximum_bus_size);

   if(has_misaligned_indirect_ref||has_unaligned_accesses)
   {
      if(maximum_bus_size>bram_bitsize_max)
         THROW_ERROR("Unsupported data bus size. In case, try a device supporting this BRAM BITSIZE: " + STR(maximum_bus_size) + " available maximum BRAM BITSIZE: " + STR(bram_bitsize_max));
      else
         bram_bitsize = maximum_bus_size;
   }
   else if(maximum_bus_size/2>bram_bitsize_max)
   {
      THROW_ERROR("Unsupported data bus size. In case, try a device supporting this BRAM BITSIZE: " + STR(maximum_bus_size/2) + " available maximum BRAM BITSIZE: " + STR(bram_bitsize_max));
   }
   else
   {
      bram_bitsize = maximum_bus_size/2;
   }

   if(bram_bitsize < bram_bitsize_min)
      bram_bitsize = bram_bitsize_min;

   if(bram_bitsize < 8)
      bram_bitsize = 8;


   if(!use_databus_width || use_unknown_address)
      aligned_bitsize = 8;
   else
      aligned_bitsize = 2*bram_bitsize;

   if(parameters->isOption(OPT_addr_bus_bitsize))
      addr_bus_bitsize = parameters->getOption<unsigned int>(OPT_addr_bus_bitsize);
   else if(use_unknown_address)
      addr_bus_bitsize = 32;
   else if(has_intern_shared_data && parameters->getOption<MemoryAllocation_Policy>(OPT_memory_allocation_policy) != MemoryAllocation_Policy::ALL_BRAM)
      addr_bus_bitsize = 32;
   else if(HLSMgr->Rmem->get_memory_address()-HLSMgr->base_address>0)
      addr_bus_bitsize = 32;
   else
   {
      unsigned int addr_range = HLSMgr->Rmem->get_max_address();
      if(addr_range)
      {
        addr_range = std::max(addr_range, ((2*HLS_T->get_target_device()->get_parameter<unsigned int>("BRAM_bitsize_max"))/8));
        --addr_range;
      }
      unsigned int index;
      for(index = 1; addr_range >= (1u<<index) ; ++index);
      addr_bus_bitsize = index;
      if(HLSMgr->Rmem->count_non_private_internal_symbols()==1)
         ++addr_bus_bitsize;
   }
   HLSMgr->Rmem->set_bus_addr_bitsize(addr_bus_bitsize);
   if(needMemoryMappedRegisters)
      maximum_bus_size = std::max(maximum_bus_size, addr_bus_bitsize);
   data_bus_bitsize = maximum_bus_size;
   HLSMgr->Rmem->set_bus_data_bitsize(data_bus_bitsize);
   for(size_bus_bitsize=4; data_bus_bitsize >= (1u<<size_bus_bitsize); ++size_bus_bitsize);
   HLSMgr->Rmem->set_bus_size_bitsize(size_bus_bitsize);

   HLSMgr->Rmem->set_aligned_bitsize(aligned_bitsize);
   HLSMgr->Rmem->set_bram_bitsize(bram_bitsize);
   HLSMgr->Rmem->set_intern_shared_data(has_intern_shared_data);
   HLSMgr->Rmem->set_use_unknown_addresses(use_unknown_address);
   HLSMgr->Rmem->set_pointer_conversion(pointer_conversion_happen);
   HLSMgr->Rmem->set_unaligned_accesses(has_unaligned_accesses);

   INDENT_OUT_MEX(OUTPUT_LEVEL_MINIMUM, output_level, "-->");
   INDENT_OUT_MEX(OUTPUT_LEVEL_MINIMUM, output_level, "---BRAM bitsize: " + STR(bram_bitsize));
   INDENT_OUT_MEX(OUTPUT_LEVEL_MINIMUM, output_level, "---" + (use_databus_width ? std::string("Spec may exploit DATA bus width") : std::string("Spec may not exploit DATA bus width")));
   INDENT_OUT_MEX(OUTPUT_LEVEL_MINIMUM, output_level, "---" + (!use_unknown_address ? std::string("All the data have a known address") : std::string("Spec accesses data having an address unknown at compile time")));
   INDENT_OUT_MEX(OUTPUT_LEVEL_MINIMUM, output_level, "---" + (!has_intern_shared_data ? std::string("Intern data is not externally accessible") : std::string("Intern data may be accessed")));
   INDENT_OUT_MEX(OUTPUT_LEVEL_MINIMUM, output_level, "---Aligned bitsize: " + STR(aligned_bitsize));
   INDENT_OUT_MEX(OUTPUT_LEVEL_MINIMUM, output_level, "---DATA bus bitsize: " + STR(data_bus_bitsize));
   INDENT_OUT_MEX(OUTPUT_LEVEL_MINIMUM, output_level, "---ADDRESS bus bitsize: " + STR(addr_bus_bitsize));
   INDENT_OUT_MEX(OUTPUT_LEVEL_MINIMUM, output_level, "---SIZE bus bitsize: " + STR(size_bus_bitsize));
   if(HLSMgr->Rmem->has_all_pointers_resolved())
      INDENT_OUT_MEX(OUTPUT_LEVEL_MINIMUM, output_level, "---ALL pointers have been resolved");
#if 0
   if(pointer_conversion_happen)
      INDENT_OUT_MEX(OUTPUT_LEVEL_MINIMUM, output_level, "At least one pointer conversion has been performed into the code");
   else
      INDENT_OUT_MEX(OUTPUT_LEVEL_MINIMUM, output_level, "No pointer recast into the code");
#endif
   if(has_unaligned_accesses)
      INDENT_OUT_MEX(OUTPUT_LEVEL_MINIMUM, output_level, "---Code has LOADs or STOREs with unaligned accesses");
   if(HLSMgr->Rmem->get_allocated_parameters_memory())
      INDENT_OUT_MEX(OUTPUT_LEVEL_MINIMUM, output_level, "---Total amount of memory allocated for memory mapped parameters: " + STR(HLSMgr->Rmem->get_allocated_parameters_memory()));
   INDENT_OUT_MEX(OUTPUT_LEVEL_MINIMUM, output_level, "---Internally allocated memory (no private memories): " + STR(HLSMgr->Rmem->get_allocated_intern_memory()));
   INDENT_OUT_MEX(OUTPUT_LEVEL_MINIMUM, output_level, "---Internally allocated memory: " + STR(HLSMgr->Rmem->get_allocated_space()));
   INDENT_OUT_MEX(OUTPUT_LEVEL_MINIMUM, output_level, "<--");
}

void memory_allocation::allocate_parameters(unsigned int functionId)
{
   const FunctionBehaviorConstRef function_behavior = HLSMgr->CGetFunctionBehavior(functionId);
   const BehavioralHelperConstRef behavioral_helper = function_behavior->CGetBehavioralHelper();
   const unsigned int function_return = behavioral_helper->GetFunctionReturnType(functionId);

   // Allocate memory for the start register.
   HLSMgr->Rmem->add_parameter(functionId, functionId, behavioral_helper->get_parameters().empty() && !function_return);
   std::string functionName =
         tree_helper::name_function(HLSMgr->get_tree_manager(), functionId);
   INDENT_OUT_MEX(OUTPUT_LEVEL_VERBOSE, output_level, "---Function: " + functionName);
   INDENT_OUT_MEX(OUTPUT_LEVEL_VERBOSE, output_level, "---Id: " + STR(functionId));
   INDENT_OUT_MEX(OUTPUT_LEVEL_VERBOSE, output_level, "---Base Address: " + STR(HLSMgr->Rmem->get_parameter_base_address(functionId, functionId)));

   // Allocate every parameter on chip.
   const std::list<unsigned int>& topParams = behavioral_helper->get_parameters();
   for (std::list<unsigned int>::const_iterator itr = topParams.begin(), end = topParams.end(); itr != end; ++itr)
   {
      std::list<unsigned int>::const_iterator itr_next = itr;
      ++itr_next;
      HLSMgr->Rmem->add_parameter(behavioral_helper->get_function_index(), *itr, !function_return && itr_next==end);
      INDENT_OUT_MEX(OUTPUT_LEVEL_VERBOSE, output_level, "-->Parameter " + behavioral_helper->PrintVariable(*itr) + " of Function " + functionName);
      INDENT_OUT_MEX(OUTPUT_LEVEL_VERBOSE, output_level, "---Id: " + STR(*itr));
      INDENT_OUT_MEX(OUTPUT_LEVEL_VERBOSE, output_level, "---Base Address: " + STR(HLSMgr->Rmem->get_parameter_base_address(functionId, *itr)));
      INDENT_OUT_MEX(OUTPUT_LEVEL_VERBOSE, output_level, "---Size: " + STR(tree_helper::size(HLSMgr->get_tree_manager(),  *itr) / 8));
      INDENT_OUT_MEX(OUTPUT_LEVEL_VERBOSE, output_level, "<--");
   }

   // Allocate the return value on chip.
   if (function_return)
   {
      HLSMgr->Rmem->add_parameter(behavioral_helper->get_function_index(), function_return, true);
      INDENT_OUT_MEX(OUTPUT_LEVEL_VERBOSE, output_level, "-->Return parameter for Function: " + functionName);
      INDENT_OUT_MEX(OUTPUT_LEVEL_VERBOSE, output_level, "---Id: " + STR(function_return));
      INDENT_OUT_MEX(OUTPUT_LEVEL_VERBOSE, output_level, "---Base Address: " + STR(HLSMgr->Rmem->get_parameter_base_address(functionId, function_return)));
      INDENT_OUT_MEX(OUTPUT_LEVEL_VERBOSE, output_level, "---Size: " + STR(tree_helper::size(HLSMgr->get_tree_manager(),  function_return) / 8));
      INDENT_OUT_MEX(OUTPUT_LEVEL_VERBOSE, output_level, "<--");
   }
}

bool memory_allocation::HasToBeExecuted() const
{
   return true;
}
