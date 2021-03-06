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
 * @file soft_float_cg_ext.hpp
 * @brief Step that extends the call graph with the soft-float calls where appropriate.
 *
 * @author Fabrizio Ferrandi <fabrizio.ferrandi@polimi.it>
 *
*/
#ifndef SOFT_FLOAT_CG_EXT_HPP
#define SOFT_FLOAT_CG_EXT_HPP

///Superclass include
#include "function_frontend_flow_step.hpp"

///STL include
#include <set>
#include <unordered_set>

///Utility include
#include "refcount.hpp"

/**
 * @name forward declarations
 */
//@{
REF_FORWARD_DECL(soft_float_cg_ext);
REF_FORWARD_DECL(tree_manager);
REF_FORWARD_DECL(tree_manipulation);
REF_FORWARD_DECL(tree_node);
//@}

/**
 * Add to the call graph the function calls associated with the floating point primitive operations
 */
class soft_float_cg_ext : public FunctionFrontendFlowStep
{
   protected:
      ///Tree manager
      const tree_managerRef TreeM;

      ///tree manipulation
      const tree_manipulationRef tree_man;

      ///when true IR has been modified
      bool modified;

      /**
      * Recursive examinate tree node
      * @param current_statement is the current analyzed statement
      * @param current_tree_node is the current tree node
      */
      void RecursiveExaminate(const tree_nodeRef current_statement, const tree_nodeRef current_tree_node);

      /**
       * Return the set of analyses in relationship with this design step
       * @param relationship_type is the type of relationship to be considered
       */
      virtual const std::unordered_set<std::pair<FrontendFlowStepType, FunctionRelationship> > ComputeFrontendRelationships(const DesignFlowStep::RelationshipType relationship_type) const;

   public:
      /**
       * Constructor.
       * @param Param is the set of the parameters
       * @param AppM is the application manager
       * @param fun_id is the function index
       * @param design_flow_manager is the design flow manager
       */
      soft_float_cg_ext(const ParameterConstRef Param, const application_managerRef AppM, unsigned int fun_id, const DesignFlowManagerConstRef design_flow_manager);

      /**
       * Destructor
       */
      virtual ~soft_float_cg_ext();

      /**
       * Fixes the var_decl duplication.
       */
      DesignFlowStep_Status InternalExec();
};

#endif

