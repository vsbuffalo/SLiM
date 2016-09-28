//
//  individual.cpp
//  SLiM
//
//  Created by Ben Haller on 6/10/16.
//  Copyright (c) 2016 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

//	This file is part of SLiM.
//
//	SLiM is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
//
//	SLiM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along with SLiM.  If not, see <http://www.gnu.org/licenses/>.


#include "individual.h"
#include "subpopulation.h"
#include "slim_sim.h"
#include "eidos_property_signature.h"
#include "eidos_call_signature.h"

#include <string>
#include <algorithm>
#include <vector>


#ifdef DEBUG
bool Individual::s_log_copy_and_assign_ = true;
#endif


// A global counter used to assign all Individual objects a unique ID
slim_mutationid_t gSLiM_next_pedigree_id = 0;


Individual::Individual(const Individual &p_original) : subpopulation_(p_original.subpopulation_), index_(p_original.index_), tag_value_(p_original.tag_value_),
	pedigree_id_(p_original.pedigree_id_), pedigree_p1_(p_original.pedigree_p1_), pedigree_p2_(p_original.pedigree_p2_),
	pedigree_g1_(p_original.pedigree_g1_), pedigree_g2_(p_original.pedigree_g2_), pedigree_g3_(p_original.pedigree_g3_), pedigree_g4_(p_original.pedigree_g4_)
{
#ifdef DEBUG
	if (s_log_copy_and_assign_)
	{
		SLIM_ERRSTREAM << "********* Individual::Individual(Individual&) called!" << std::endl;
		eidos_print_stacktrace(stderr);
		SLIM_ERRSTREAM << "************************************************" << std::endl;
	}
#endif
}

#ifdef DEBUG
bool Individual::LogIndividualCopyAndAssign(bool p_log)
{
	bool old_value = s_log_copy_and_assign_;
	
	s_log_copy_and_assign_ = p_log;
	
	return old_value;
}
#endif

Individual::Individual(Subpopulation &p_subpopulation, slim_popsize_t p_individual_index) : subpopulation_(p_subpopulation), index_(p_individual_index),
	pedigree_id_(-1), pedigree_p1_(-1), pedigree_p2_(-1), pedigree_g1_(-1), pedigree_g2_(-1), pedigree_g3_(-1), pedigree_g4_(-1)
{
	// pedigree_id_(gSLiM_next_pedigree_id++) makes it so that new Individual objects generated by
	// Subpopulation::GenerateChildrenToFit() already have an id set up; otherwise, we would have
	// to wait a generation for new children to receive ID values, slowing down pedigree analysis
	// by a generation.  We only do this if pedigrees are enabled; it takes a bit of time to find
	// that out, but it only happens when the Individual vectors are first set up, and it lets us
	// guarantee that pedigree_id_ is -1 when pedigree tracking is not enabled.
	if (subpopulation_.population_.sim_.PedigreesEnabled())
		pedigree_id_ = gSLiM_next_pedigree_id++;
}

Individual::~Individual(void)
{
}

double Individual::RelatednessToIndividual(Individual &ind)
{
	// If we're being asked about ourselves, return 1.0, even if pedigree tracking is off
	if (this == &ind)
		return 1.0;
	
	// Otherwise, if our own pedigree information is not initialized, then we have nothing to go on
	if (pedigree_id_ == -1)
		return 0.0;
	
	// Start with 0.0 and add in factors for shared ancestors
	double relatedness = 0.0;
	
	if ((pedigree_g1_ != -1) && (ind.pedigree_g1_ != -1))
	{
		// We have grandparental information, so use that; that will be the most accurate
		double g1 = pedigree_g1_;
		double g2 = pedigree_g2_;
		double g3 = pedigree_g3_;
		double g4 = pedigree_g4_;
		
		double ind_g1 = ind.pedigree_g1_;
		double ind_g2 = ind.pedigree_g2_;
		double ind_g3 = ind.pedigree_g3_;
		double ind_g4 = ind.pedigree_g4_;
		
		// Each shared grandparent adds 0.125, for a maximum of 0.5
		if ((g1 == ind_g1) || (g1 == ind_g2) || (g1 == ind_g3) || (g1 == ind_g4))	relatedness += 0.125;
		if ((g2 == ind_g1) || (g2 == ind_g2) || (g2 == ind_g3) || (g2 == ind_g4))	relatedness += 0.125;
		if ((g3 == ind_g1) || (g3 == ind_g2) || (g3 == ind_g3) || (g3 == ind_g4))	relatedness += 0.125;
		if ((g4 == ind_g1) || (g4 == ind_g2) || (g4 == ind_g3) || (g4 == ind_g4))	relatedness += 0.125;
	}
	else if ((pedigree_p1_ != -1) && (ind.pedigree_p1_ != -1))
	{
		// We have parental information; that's second-best
		double p1 = pedigree_p1_;
		double p2 = pedigree_p2_;
		
		double ind_p1 = ind.pedigree_p1_;
		double ind_p2 = ind.pedigree_p2_;
		
		// Each shared parent adds 0.25, for a maximum of 0.5
		if ((p1 == ind_p1) || (p1 == ind_p2))	relatedness += 0.25;
		if ((p2 == ind_p1) || (p2 == ind_p2))	relatedness += 0.25;
	}
	
	// With no information, we assume we are not related
	return relatedness;
}


//
// Eidos support
//
#pragma mark -
#pragma mark Eidos support

void Individual::GenerateCachedEidosValue(void)
{
	// Note that this cache cannot be invalidated, because we are guaranteeing that this object will
	// live for at least as long as the symbol table it may be placed into!
	self_value_ = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_Individual_Class));
}

const EidosObjectClass *Individual::Class(void) const
{
	return gSLiM_Individual_Class;
}

void Individual::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ElementType() << "<p" << subpopulation_.subpopulation_id_ << ":i" << index_ << ">";
}

void Individual::GetGenomes(Genome **p_genome1, Genome **p_genome2) const
{
	// The way we get our genomes is really disgusting, so it is localized here in a utility method.  The problem is that we could
	// represent an individual in either the child or the parental generation.  The subpopulation's child_generation_valid_ flag
	// is not the correct way to decide, because in some contexts, such as modifyChild() callbacks, Individual objects for both
	// generations are being handled.  We don't want to contain a flag for which generation we refer to, because when the generation
	// flips all of those flags would have to be flipped, which would just be a big waste of time.  So instead, we do an evil hack:
	// we do pointer comparisons to determine which vector of Individuals in the subpopulation we belong to.  On the bright side,
	// this also represents a sort of integrity checkback, since we will raise if we can't find ourselves.
	std::vector<Individual> &parent_individuals = subpopulation_.parent_individuals_;
	std::vector<Individual> &child_individuals = subpopulation_.child_individuals_;
	bool is_parent = ((this >= &(parent_individuals.front())) && (this <= &(parent_individuals.back())));
	bool is_child = ((this >= &(child_individuals.front())) && (this <= &(child_individuals.back())));
	
	std::vector<Genome> *genomes;
	
	if (is_parent && !is_child)
		genomes = &subpopulation_.parent_genomes_;
	else if (is_child && !is_parent)
		genomes = &subpopulation_.child_genomes_;
	else
		EIDOS_TERMINATION << "ERROR (Individual::GetGenomes): (internal error) unable to unambiguously find genomes." << eidos_terminate();
	
	Genome *genome1, *genome2;
	int genome_count = (int)genomes->size();
	slim_popsize_t genome_index = index_ * 2;
	
	if (genome_index + 1 < genome_count)
	{
		genome1 = &((*genomes)[genome_index]);
		genome2 = &((*genomes)[genome_index + 1]);
	}
	else
	{
		genome1 = nullptr;
		genome2 = nullptr;
	}
	
	if (p_genome1)
		*p_genome1 = genome1;
	if (p_genome2)
		*p_genome2 = genome2;
}

IndividualSex Individual::Sex(void) const
{
	if (subpopulation_.sex_enabled_)
	{
		// See Individual::GetGenomes() above for general comments about why this approach is necessary and how it works.
		std::vector<Individual> &parent_individuals = subpopulation_.parent_individuals_;
		std::vector<Individual> &child_individuals = subpopulation_.child_individuals_;
		bool is_parent = ((this >= &(parent_individuals.front())) && (this <= &(parent_individuals.back())));
		bool is_child = ((this >= &(child_individuals.front())) && (this <= &(child_individuals.back())));
		
		if (is_parent && !is_child)
			return ((index_ < subpopulation_.parent_first_male_index_) ? IndividualSex::kFemale : IndividualSex::kMale);
		else if (is_child && !is_parent)
			return ((index_ < subpopulation_.child_first_male_index_) ? IndividualSex::kFemale : IndividualSex::kMale);
		else
			EIDOS_TERMINATION << "ERROR (Individual::Sex): (internal error) unable to unambiguously find genomes." << eidos_terminate();
	}
	else
	{
		// If sex is not enabled, the question is easy to answer
		return IndividualSex::kHermaphrodite;
	}
}

EidosValue_SP Individual::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_subpopulation:		// ACCELERATED
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(&subpopulation_, gSLiM_Subpopulation_Class));
		}
		case gID_index:				// ACCELERATED
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(index_));
		}
		case gID_genomes:
		{
			Genome *genome1, *genome2;
			
			GetGenomes(&genome1, &genome2);
			
			if (genome1 && genome2)
			{
				EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Genome_Class))->Reserve(2);
				
				vec->PushObjectElement(genome1);
				vec->PushObjectElement(genome2);
				
				return EidosValue_SP(vec);
			}
			else
			{
				return gStaticEidosValueNULL;
			}
		}
		case gID_sex:
		{
			static EidosValue_SP static_sex_string_H;
			static EidosValue_SP static_sex_string_F;
			static EidosValue_SP static_sex_string_M;
			static EidosValue_SP static_sex_string_O;
			
			if (!static_sex_string_H)
			{
				static_sex_string_H = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("H"));
				static_sex_string_F = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("F"));
				static_sex_string_M = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("M"));
				static_sex_string_O = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("?"));
			}
			
			switch (Sex())
			{
				case IndividualSex::kHermaphrodite:	return static_sex_string_H;
				case IndividualSex::kFemale:		return static_sex_string_F;
				case IndividualSex::kMale:			return static_sex_string_M;
				default:							return static_sex_string_O;
			}
		}
		case gID_pedigreeID:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(pedigree_id_));
		}
		case gID_pedigreeParentIDs:
		{
			EidosValue_Int_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve(2);
			
			vec->PushInt(pedigree_p1_);
			vec->PushInt(pedigree_p2_);
			
			return EidosValue_SP(vec);
		}
		case gID_pedigreeGrandparentIDs:
		{
			EidosValue_Int_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->Reserve(4);
			
			vec->PushInt(pedigree_g1_);
			vec->PushInt(pedigree_g2_);
			vec->PushInt(pedigree_g2_);
			vec->PushInt(pedigree_g2_);
			
			return EidosValue_SP(vec);
		}
		case gID_uniqueMutations:
		{
			Genome *genome1, *genome2;
			
			GetGenomes(&genome1, &genome2);
			
			if (genome1 && genome2)
			{
				Genome &g1 = *genome1, &g2 = *genome2;
				
				// We reserve a vector large enough to hold all the mutations from both genomes; probably usually overkill, but it does little harm
				int g1_size = (g1.IsNull() ? 0 : g1.size()), g2_size = (g2.IsNull() ? 0 : g2.size());
				EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Mutation_Class))->Reserve(g1_size + g2_size);
				EidosValue_SP result_SP = EidosValue_SP(vec);
				
				// We want to interleave mutations from the two genomes, keeping only the uniqued mutations.  For a given position, we take mutations
				// from g1 first, and then look at the mutations in g2 at the same position and add them if they are not in g1.
				int g1_index = 0, g2_index = 0;
				
				if (g1_size && g2_size)
				{
					// Get the position of the mutations at g1_index and g2_index
					Mutation *g1_mut = g1[g1_index], *g2_mut = g2[g2_index];
					slim_position_t pos1 = g1_mut->position_, pos2 = g2_mut->position_;
					
					// Process mutations as long as both genomes still have mutations left in them
					do
					{
						if (pos1 < pos2)
						{
							vec->PushObjectElement(g1_mut);
							
							// Move to the next mutation in g1
							if (++g1_index >= g1_size)
								break;
							g1_mut = g1[g1_index];
							pos1 = g1_mut->position_;
						}
						else if (pos1 > pos2)
						{
							vec->PushObjectElement(g2_mut);
							
							// Move to the next mutation in g2
							if (++g2_index >= g2_size)
								break;
							g2_mut = g2[g2_index];
							pos2 = g2_mut->position_;
						}
						else
						{
							// pos1 == pos2; copy mutations from g1 until we are done with this position, then handle g2
							slim_position_t focal_pos = pos1;
							int first_index = g1_index;
							bool done = false;
							
							while (pos1 == focal_pos)
							{
								vec->PushObjectElement(g1_mut);
								
								// Move to the next mutation in g1
								if (++g1_index >= g1_size)
								{
									done = true;
									break;
								}
								g1_mut = g1[g1_index];
								pos1 = g1_mut->position_;
							}
							
							// Note that we may be done with g1 here, so be careful
							int last_index_plus_one = g1_index;
							
							while (pos2 == focal_pos)
							{
								int check_index;
								
								for (check_index = first_index; check_index < last_index_plus_one; ++check_index)
									if (g1[check_index] == g2_mut)
										break;
								
								// If the check indicates that g2_mut is not in g1, we copy it over
								if (check_index == last_index_plus_one)
									vec->PushObjectElement(g2_mut);
								
								// Move to the next mutation in g2
								if (++g2_index >= g2_size)
								{
									done = true;
									break;
								}
								g2_mut = g2[g2_index];
								pos2 = g2_mut->position_;
							}
							
							// Note that we may be done with both g1 and/or g2 here; if so, done will be set and we will break out
							if (done)
								break;
						}
					}
					while (true);
				}
				
				// Finish off any tail ends, which must be unique and sorted already
				while (g1_index < g1_size)
					vec->PushObjectElement(g1[g1_index++]);
				while (g2_index < g2_size)
					vec->PushObjectElement(g2[g2_index++]);
				
				return result_SP;
			}
			else
			{
				return gStaticEidosValueNULL;
			}
			/*
			 The code above for uniqueMutations can be tested with the simple SLiM script below.  Positions are tested with
			 identical() instead of the mutation vectors themselves, only because the sorted order of mutations at exactly
			 the same position may differ; identical(um1, um2) will occasionally flag these as false positives.
			 
			 initialize() {
				 initializeMutationRate(1e-5);
				 initializeMutationType("m1", 0.5, "f", 0.0);
				 initializeGenomicElementType("g1", m1, 1.0);
				 initializeGenomicElement(g1, 0, 99999);
				 initializeRecombinationRate(1e-8);
			 }
			 1 {
				sim.addSubpop("p1", 500);
			 }
			 1:20000 late() {
				 for (i in p1.individuals)
				 {
					 um1 = i.uniqueMutations;
					 um2 = sortBy(unique(i.genomes.mutations), "position");
					 
					 if (!identical(um1.position, um2.position))
					 {
						 print("Mismatch!");
						 print(um1.position);
						 print(um2.position);
					 }
				 }
			 }
			 */
		}
			
			// variables
		case gID_tag:				// ACCELERATED
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value_));
		}
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

int64_t Individual::GetProperty_Accelerated_Int(EidosGlobalStringID p_property_id)
{
	switch (p_property_id)
	{
		case gID_index:			return index_;
		case gID_pedigreeID:	return pedigree_id_;
		case gID_tag:			return tag_value_;
			
		default:				return EidosObjectElement::GetProperty_Accelerated_Int(p_property_id);
	}
}

EidosObjectElement *Individual::GetProperty_Accelerated_ObjectElement(EidosGlobalStringID p_property_id)
{
	switch (p_property_id)
	{
		case gID_subpopulation:	return &subpopulation_;
			
		default:				return EidosObjectElement::GetProperty_Accelerated_ObjectElement(p_property_id);
	}
}

void Individual::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_tag:
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex(0, nullptr));
			
			tag_value_ = value;
			return;
		}
			
			// all others, including gID_none
		default:
			return EidosObjectElement::SetProperty(p_property_id, p_value);
	}
}

EidosValue_SP Individual::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue *arg0_value = ((p_argument_count >= 1) ? p_arguments[0].get() : nullptr);
	
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
			//
			//	*********************	- (logical)containsMutations(object<Mutation> mutations)
			//
#pragma mark -containsMutations()
			
		case gID_containsMutations:
		{
			Genome *genome1, *genome2;
			
			GetGenomes(&genome1, &genome2);
			
			if (genome1 && genome2)
			{
				int arg0_count = arg0_value->Count();
				
				if (arg0_count == 1)
				{
					Mutation *mut = (Mutation *)(arg0_value->ObjectElementAtIndex(0, nullptr));
					
					if ((!genome1->IsNull() && genome1->contains_mutation(mut)) || (!genome2->IsNull() && genome2->contains_mutation(mut)))
						return gStaticEidosValue_LogicalT;
					else
						return gStaticEidosValue_LogicalF;
				}
				else
				{
					EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->Reserve(arg0_count);
					std::vector<eidos_logical_t> &logical_result_vec = *logical_result->LogicalVector_Mutable();
					
					for (int value_index = 0; value_index < arg0_count; ++value_index)
					{
						Mutation *mut = (Mutation *)(arg0_value->ObjectElementAtIndex(value_index, nullptr));
						bool contains_mut = ((!genome1->IsNull() && genome1->contains_mutation(mut)) || (!genome2->IsNull() && genome2->contains_mutation(mut)));
						
						logical_result_vec.emplace_back(contains_mut);
					}
					
					return EidosValue_SP(logical_result);
				}
			}
			
			return gStaticEidosValueNULL;
		}
			
			
			//
			//	*********************	- (integer$)countOfMutationsOfType(io<MutationType>$ mutType)
			//
#pragma mark -countOfMutationsOfType()
			
		case gID_countOfMutationsOfType:
		{
			Genome *genome1, *genome2;
			
			GetGenomes(&genome1, &genome2);
			
			if (genome1 && genome2)
			{
				MutationType *mutation_type_ptr = nullptr;
				
				if (arg0_value->Type() == EidosValueType::kValueInt)
				{
					SLiMSim *sim = dynamic_cast<SLiMSim *>(p_interpreter.Context());
					
					if (!sim)
						EIDOS_TERMINATION << "ERROR (Individual::ExecuteInstanceMethod): (internal error) the sim is not registered as the context pointer." << eidos_terminate();
					
					slim_objectid_t mutation_type_id = SLiMCastToObjectidTypeOrRaise(arg0_value->IntAtIndex(0, nullptr));
					auto found_muttype_pair = sim->MutationTypes().find(mutation_type_id);
					
					if (found_muttype_pair == sim->MutationTypes().end())
						EIDOS_TERMINATION << "ERROR (Individual::ExecuteInstanceMethod): countOfMutationsOfType() mutation type m" << mutation_type_id << " not defined." << eidos_terminate();
					
					mutation_type_ptr = found_muttype_pair->second;
				}
				else
				{
					mutation_type_ptr = (MutationType *)(arg0_value->ObjectElementAtIndex(0, nullptr));
				}
				
				// Count the number of mutations of the given type
				int match_count = 0;
				
				if (!genome1->IsNull())
				{
					int genome1_count = genome1->size();
					Mutation **genome1_ptr = genome1->begin_pointer();
					
					for (int mut_index = 0; mut_index < genome1_count; ++mut_index)
						if (genome1_ptr[mut_index]->mutation_type_ptr_ == mutation_type_ptr)
							++match_count;
				}
				if (!genome2->IsNull())
				{
					int genome2_count = genome2->size();
					Mutation **genome2_ptr = genome2->begin_pointer();
					
					for (int mut_index = 0; mut_index < genome2_count; ++mut_index)
						if (genome2_ptr[mut_index]->mutation_type_ptr_ == mutation_type_ptr)
							++match_count;
				}
				
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(match_count));
			}
			
			return gStaticEidosValueNULL;
		}
			
			
			//
			//	*********************	- (float$)relatedness(o<Individual>$ individuals)
			//
#pragma mark -relatedness()
			
		case gID_relatedness:
		{
			int arg0_count = arg0_value->Count();
			
			if (arg0_count == 1)
			{
				Individual *ind = (Individual *)(arg0_value->ObjectElementAtIndex(0, nullptr));
				double relatedness = RelatednessToIndividual(*ind);
				
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(relatedness));
			}
			else
			{
				EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(arg0_count);
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
				{
					Individual *ind = (Individual *)(arg0_value->ObjectElementAtIndex(value_index, nullptr));
					double relatedness = RelatednessToIndividual(*ind);
					
					float_result->PushFloat(relatedness);
				}
				
				return EidosValue_SP(float_result);
			}
			
			return gStaticEidosValueNULL;
		}
			
			
			//
			//	*********************	- (object<Mutation>)uniqueMutationsOfType(io<MutationType>$ mutType)
			//
#pragma mark -uniqueMutationsOfType()
			
		case gID_uniqueMutationsOfType:
		{
			MutationType *mutation_type_ptr = nullptr;
			
			if (arg0_value->Type() == EidosValueType::kValueInt)
			{
				SLiMSim *sim = dynamic_cast<SLiMSim *>(p_interpreter.Context());
				
				if (!sim)
					EIDOS_TERMINATION << "ERROR (Individual::ExecuteInstanceMethod): (internal error) the sim is not registered as the context pointer." << eidos_terminate();
				
				slim_objectid_t mutation_type_id = SLiMCastToObjectidTypeOrRaise(arg0_value->IntAtIndex(0, nullptr));
				auto found_muttype_pair = sim->MutationTypes().find(mutation_type_id);
				
				if (found_muttype_pair == sim->MutationTypes().end())
					EIDOS_TERMINATION << "ERROR (Individual::ExecuteInstanceMethod): uniqueMutationsOfType() mutation type m" << mutation_type_id << " not defined." << eidos_terminate();
				
				mutation_type_ptr = found_muttype_pair->second;
			}
			else
			{
				mutation_type_ptr = (MutationType *)(arg0_value->ObjectElementAtIndex(0, nullptr));
			}
			
			// This code is adapted from uniqueMutations and follows its logic closely
			Genome *genome1, *genome2;
			
			GetGenomes(&genome1, &genome2);
			
			if (genome1 && genome2)
			{
				Genome &g1 = *genome1, &g2 = *genome2;
				
				// We try to reserve a vector large enough to hold all the mutations; probably usually overkill, but it does little harm
				int g1_size = (g1.IsNull() ? 0 : g1.size()), g2_size = (g2.IsNull() ? 0 : g2.size());
				EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Mutation_Class));
				EidosValue_SP result_SP = EidosValue_SP(vec);
				
				if (g1_size + g2_size < 100)	// an arbitrary limit, but we don't want to make something *too* unnecessarily big...
					vec->Reserve(g1_size + g2_size);
				
				// We want to interleave mutations from the two genomes, keeping only the uniqued mutations.  For a given position, we take mutations
				// from g1 first, and then look at the mutations in g2 at the same position and add them if they are not in g1.
				int g1_index = 0, g2_index = 0;
				
				if (g1_size && g2_size)
				{
					Mutation *g1_mut = g1[g1_index], *g2_mut = g2[g2_index];
					
					// At this point, we need to loop forward in g1 and g2 until we have found mutations of the right type in both
					while (g1_mut->mutation_type_ptr_ != mutation_type_ptr)
					{
						if (++g1_index >= g1_size)
							break;
						g1_mut = g1[g1_index];
					}
					
					while (g2_mut->mutation_type_ptr_ != mutation_type_ptr)
					{
						if (++g2_index >= g2_size)
							break;
						g2_mut = g2[g2_index];
					}
					
					if ((g1_index < g1_size) && (g2_index < g2_size))
					{
						slim_position_t pos1 = g1_mut->position_;
						slim_position_t pos2 = g2_mut->position_;
						
						// Process mutations as long as both genomes still have mutations left in them
						do
						{
							// Now we have mutations of the right type, so we can start working with them by position
							if (pos1 < pos2)
							{
								vec->PushObjectElement(g1_mut);
								
								// Move to the next mutation in g1
							loopback1:
								if (++g1_index >= g1_size)
									break;
								
								g1_mut = g1[g1_index];
								if (g1_mut->mutation_type_ptr_ != mutation_type_ptr)
									goto loopback1;
								
								pos1 = g1_mut->position_;
							}
							else if (pos1 > pos2)
							{
								vec->PushObjectElement(g2_mut);
								
								// Move to the next mutation in g2
							loopback2:
								if (++g2_index >= g2_size)
									break;
								
								g2_mut = g2[g2_index];
								if (g2_mut->mutation_type_ptr_ != mutation_type_ptr)
									goto loopback2;
								
								pos2 = g2_mut->position_;
							}
							else
							{
								// pos1 == pos2; copy mutations from g1 until we are done with this position, then handle g2
								slim_position_t focal_pos = pos1;
								int first_index = g1_index;
								bool done = false;
								
								while (pos1 == focal_pos)
								{
									vec->PushObjectElement(g1_mut);
									
									// Move to the next mutation in g1
								loopback3:
									if (++g1_index >= g1_size)
									{
										done = true;
										break;
									}
									g1_mut = g1[g1_index];
									if (g1_mut->mutation_type_ptr_ != mutation_type_ptr)
										goto loopback3;
									
									pos1 = g1_mut->position_;
								}
								
								// Note that we may be done with g1 here, so be careful
								int last_index_plus_one = g1_index;
								
								while (pos2 == focal_pos)
								{
									int check_index;
									
									for (check_index = first_index; check_index < last_index_plus_one; ++check_index)
										if (g1[check_index] == g2_mut)
											break;
									
									// If the check indicates that g2_mut is not in g1, we copy it over
									if (check_index == last_index_plus_one)
										vec->PushObjectElement(g2_mut);
									
									// Move to the next mutation in g2
								loopback4:
									if (++g2_index >= g2_size)
									{
										done = true;
										break;
									}
									g2_mut = g2[g2_index];
									if (g2_mut->mutation_type_ptr_ != mutation_type_ptr)
										goto loopback4;
									
									pos2 = g2_mut->position_;
								}
								
								// Note that we may be done with both g1 and/or g2 here; if so, done will be set and we will break out
								if (done)
									break;
							}
						}
						while (true);
					}
				}
				
				// Finish off any tail ends, which must be unique and sorted already
				while (g1_index < g1_size)
				{
					Mutation *mut = g1[g1_index++];
					
					if (mut->mutation_type_ptr_ == mutation_type_ptr)
						vec->PushObjectElement(mut);
				}
				while (g2_index < g2_size)
				{
					Mutation *mut = g2[g2_index++];
					
					if (mut->mutation_type_ptr_ == mutation_type_ptr)
						vec->PushObjectElement(mut);
				}
				
				return result_SP;
			}
			else
			{
				return gStaticEidosValueNULL;
			}
			/*
			 A SLiM model to test the above code:
			 
			 initialize() {
				 initializeMutationRate(1e-5);
				 initializeMutationType("m1", 0.5, "f", 0.0);
				 initializeMutationType("m2", 0.5, "f", 0.0);
				 initializeMutationType("m3", 0.5, "f", 0.0);
				 initializeGenomicElementType("g1", c(m1, m2, m3), c(1.0, 1.0, 1.0));
				 initializeGenomicElement(g1, 0, 99999);
				 initializeRecombinationRate(1e-8);
			 }
			 1 {
				 sim.addSubpop("p1", 500);
			 }
			 1:20000 late() {
				 for (i in p1.individuals)
				 {
					 // check m1
					 um1 = i.uniqueMutationsOfType(m1);
					 um2 = sortBy(unique(i.genomes.mutationsOfType(m1)), "position");
					 
					 if (!identical(um1.position, um2.position))
					 {
						 print("Mismatch for m1!");
						 print(um1.position);
						 print(um2.position);
					 }
				 }
			 }
			 */
		}
			
			
			// all others, including gID_none
		default:
			return EidosObjectElement::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
	}
}


//
//	Individual_Class
//
#pragma mark -
#pragma mark Individual_Class

class Individual_Class : public EidosObjectClass
{
public:
	Individual_Class(const Individual_Class &p_original) = delete;	// no copy-construct
	Individual_Class& operator=(const Individual_Class&) = delete;	// no copying
	
	Individual_Class(void);
	
	virtual const std::string &ElementType(void) const;
	
	virtual const std::vector<const EidosPropertySignature *> *Properties(void) const;
	virtual const EidosPropertySignature *SignatureForProperty(EidosGlobalStringID p_property_id) const;
	
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
	virtual const EidosMethodSignature *SignatureForMethod(EidosGlobalStringID p_method_id) const;
	virtual EidosValue_SP ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
};

EidosObjectClass *gSLiM_Individual_Class = new Individual_Class();


Individual_Class::Individual_Class(void)
{
}

const std::string &Individual_Class::ElementType(void) const
{
	return gStr_Individual;
}

const std::vector<const EidosPropertySignature *> *Individual_Class::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*EidosObjectClass::Properties());
		properties->emplace_back(SignatureForPropertyOrRaise(gID_subpopulation));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_index));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_genomes));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_sex));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_tag));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_pedigreeID));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_pedigreeParentIDs));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_pedigreeGrandparentIDs));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_uniqueMutations));
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const EidosPropertySignature *Individual_Class::SignatureForProperty(EidosGlobalStringID p_property_id) const
{
	// Signatures are all preallocated, for speed
	static EidosPropertySignature *subpopulationSig = nullptr;
	static EidosPropertySignature *indexSig = nullptr;
	static EidosPropertySignature *genomesSig = nullptr;
	static EidosPropertySignature *sexSig = nullptr;
	static EidosPropertySignature *tagSig = nullptr;
	static EidosPropertySignature *pedigreeIDSig = nullptr;
	static EidosPropertySignature *pedigreeParentIDsSig = nullptr;
	static EidosPropertySignature *pedigreeGrandparentIDsSig = nullptr;
	static EidosPropertySignature *uniqueMutationsSig = nullptr;
	
	if (!subpopulationSig)
	{
		subpopulationSig =				(EidosPropertySignature *)(new EidosPropertySignature(gStr_subpopulation,			gID_subpopulation,				true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Subpopulation_Class))->DeclareAccelerated();
		indexSig =						(EidosPropertySignature *)(new EidosPropertySignature(gStr_index,					gID_index,						true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAccelerated();
		genomesSig =					(EidosPropertySignature *)(new EidosPropertySignature(gStr_genomes,					gID_genomes,					true,	kEidosValueMaskObject, gSLiM_Genome_Class));
		sexSig =						(EidosPropertySignature *)(new EidosPropertySignature(gStr_sex,						gID_sex,						true,	kEidosValueMaskString | kEidosValueMaskSingleton));
		tagSig =						(EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,						gID_tag,						false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAccelerated();
		pedigreeIDSig =					(EidosPropertySignature *)(new EidosPropertySignature(gStr_pedigreeID,				gID_pedigreeID,					true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAccelerated();
		pedigreeParentIDsSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_pedigreeParentIDs,		gID_pedigreeParentIDs,			true,	kEidosValueMaskInt));
		pedigreeGrandparentIDsSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_pedigreeGrandparentIDs,	gID_pedigreeGrandparentIDs,		true,	kEidosValueMaskInt));
		uniqueMutationsSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_uniqueMutations,			gID_uniqueMutations,			true,	kEidosValueMaskObject, gSLiM_Mutation_Class));
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_subpopulation:				return subpopulationSig;
		case gID_index:						return indexSig;
		case gID_genomes:					return genomesSig;
		case gID_sex:						return sexSig;
		case gID_tag:						return tagSig;
		case gID_pedigreeID:				return pedigreeIDSig;
		case gID_pedigreeParentIDs:			return pedigreeParentIDsSig;
		case gID_pedigreeGrandparentIDs:	return pedigreeGrandparentIDsSig;
		case gID_uniqueMutations:			return uniqueMutationsSig;
			
			// all others, including gID_none
		default:
			return EidosObjectClass::SignatureForProperty(p_property_id);
	}
}

const std::vector<const EidosMethodSignature *> *Individual_Class::Methods(void) const
{
	static std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*EidosObjectClass::Methods());
		methods->emplace_back(SignatureForMethodOrRaise(gID_containsMutations));
		methods->emplace_back(SignatureForMethodOrRaise(gID_countOfMutationsOfType));
		methods->emplace_back(SignatureForMethodOrRaise(gID_relatedness));
		methods->emplace_back(SignatureForMethodOrRaise(gID_uniqueMutationsOfType));
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const EidosMethodSignature *Individual_Class::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	static EidosInstanceMethodSignature *containsMutationsSig = nullptr;
	static EidosInstanceMethodSignature *countOfMutationsOfTypeSig = nullptr;
	static EidosInstanceMethodSignature *relatednessSig = nullptr;
	static EidosInstanceMethodSignature *uniqueMutationsOfTypeSig = nullptr;
	
	if (!containsMutationsSig)
	{
		containsMutationsSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_containsMutations, kEidosValueMaskLogical))->AddObject("mutations", gSLiM_Mutation_Class);
		countOfMutationsOfTypeSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_countOfMutationsOfType, kEidosValueMaskInt | kEidosValueMaskSingleton))->AddIntObject_S("mutType", gSLiM_MutationType_Class);
		relatednessSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_relatedness, kEidosValueMaskFloat))->AddObject("individuals", gSLiM_Individual_Class);
		uniqueMutationsOfTypeSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_uniqueMutationsOfType, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddIntObject_S("mutType", gSLiM_MutationType_Class);
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gID_containsMutations:			return containsMutationsSig;
		case gID_countOfMutationsOfType:	return countOfMutationsOfTypeSig;
		case gID_relatedness:				return relatednessSig;
		case gID_uniqueMutationsOfType:		return uniqueMutationsOfTypeSig;
			
			// all others, including gID_none
		default:
			return EidosObjectClass::SignatureForMethod(p_method_id);
	}
}

EidosValue_SP Individual_Class::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
	return EidosObjectClass::ExecuteClassMethod(p_method_id, p_target, p_arguments, p_argument_count, p_interpreter);
}












































