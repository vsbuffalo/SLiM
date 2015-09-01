//
//  population.h
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/software/
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

/*
 
 The class Population represents the entire simulated population as a map of one or more subpopulations.  This class is where much
 of the simulation logic resides; the population is called to put events into effect, to evolve, and so forth.
 
 */

#ifndef __SLiM__population__
#define __SLiM__population__


#include <vector>
#include <map>
#include <string>

#include "slim_global.h"
#include "subpopulation.h"
#include "substitution.h"
#include "chromosome.h"
#include "polymorphism.h"
#include "slim_global.h"
#include "slim_eidos_block.h"


class SLiMSim;


class Population : public std::map<slim_objectid_t,Subpopulation*>		// OWNED POINTERS
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.

public:
	
	SLiMSim &sim_;											// We have a reference back to our simulation
	Genome mutation_registry_;								// OWNED POINTERS: a registry of all mutations that have been added to this population
	slim_refcount_t total_genome_count_ = 0;				// the number of modeled genomes in the population; a fixed mutation has this frequency
#ifdef SLIMGUI
	slim_refcount_t gui_total_genome_count_ = 0;			// the number of modeled genomes in the selected subpopulations in SLiMgui
#endif
	
	std::vector<Substitution*> substitutions_;				// OWNED POINTERS: Substitution objects for all fixed mutations
	bool child_generation_valid_ = false;					// this keeps track of whether children have been generated by EvolveSubpopulation() yet, or whether the parents are still in charge
	
#ifdef SLIMGUI
	// information-gathering for various graphs in SLiMgui
	slim_generation_t *mutation_loss_times_ = nullptr;		// histogram bins: {1 bin per mutation-type} for 10 generations, realloced outward to add new generation bins as needed
	uint32_t mutation_loss_gen_slots_ = 0;					// the number of generation-sized slots (with bins per mutation-type) presently allocated
	slim_generation_t *mutation_fixation_times_ = nullptr;	// histogram bins: {1 bin per mutation-type} for 10 generations, realloced outward to add new generation bins as needed
	uint32_t mutation_fixation_gen_slots_ = 0;				// the number of generation-sized slots (with bins per mutation-type) presently allocated
	double *fitness_history_ = nullptr;						// mean fitness, recorded per generation; generation 1 goes at index 0
	slim_generation_t fitness_history_length_ = 0;			// the number of entries in the fitnessHistory buffer
#endif
	
	Population(const Population&) = delete;					// no copying
	Population& operator=(const Population&) = delete;		// no copying
	Population(void) = delete;								// no default constructor
	Population(SLiMSim &p_sim);								// our constructor: we must have a reference to our simulation
	~Population(void);										// destructor
	
	// add new empty subpopulation p_subpop_id of size p_subpop_size
	Subpopulation *AddSubpopulation(slim_objectid_t p_subpop_id, slim_popsize_t p_subpop_size, double p_initial_sex_ratio);
	
	// add new subpopulation p_subpop_id of size p_subpop_size individuals drawn from source subpopulation p_source_subpop_id
	Subpopulation *AddSubpopulation(slim_objectid_t p_subpop_id, Subpopulation &p_source_subpop, slim_popsize_t p_subpop_size, double p_initial_sex_ratio);
	
	// set size of subpopulation p_subpop_id to p_subpop_size
	void SetSize(Subpopulation &p_subpop, slim_popsize_t p_subpop_size);
	
	// set fraction p_migrant_fraction of p_subpop_id that originates as migrants from p_source_subpop_id per generation  
	void SetMigration(Subpopulation &p_subpop, slim_objectid_t p_source_subpop_id, double p_migrant_fraction);
	
	// execute a script event in the population; the script is assumed to be due to trigger
	void ExecuteScript(SLiMEidosBlock *p_script_block, slim_generation_t p_generation, const Chromosome &p_chromosome);
	
	// apply mateChoice() callbacks to a mating event with a chosen first parent; the return is the second parent index, or -1 to force a redraw
	slim_popsize_t ApplyMateChoiceCallbacks(slim_popsize_t p_parent1_index, Subpopulation *p_subpop, Subpopulation *p_source_subpop, std::vector<SLiMEidosBlock*> &p_mate_choice_callbacks);
	
	// apply modifyChild() callbacks to a generated child; a return of false means "do not use this child, generate a new one"
	bool ApplyModifyChildCallbacks(slim_popsize_t p_child_index, IndividualSex p_child_sex, slim_popsize_t p_parent1_index, slim_popsize_t p_parent2_index, bool p_is_selfing, bool p_is_cloning, Subpopulation *p_subpop, Subpopulation *p_source_subpop, std::vector<SLiMEidosBlock*> &p_modify_child_callbacks);
	
	// generate children for subpopulation p_subpop_id, drawing from all source populations, handling crossover and mutation
	void EvolveSubpopulation(Subpopulation &p_subpop, const Chromosome &p_chromosome, slim_generation_t p_generation, bool p_mate_choice_callbacks_present, bool p_modify_child_callbacks_present);
	
	// generate a child genome from parental genomes, with recombination, gene conversion, and mutation
	void DoCrossoverMutation(Subpopulation *subpop, Subpopulation *source_subpop, slim_popsize_t p_child_genome_index, slim_objectid_t p_source_subpop_id, slim_popsize_t p_parent1_genome_index, slim_popsize_t p_parent2_genome_index, const Chromosome &p_chromosome, slim_generation_t p_generation, IndividualSex p_child_sex);
	
	// generate a child genome from a single parental genome, without recombination or gene conversion, but with mutation
	void DoClonalMutation(Subpopulation *subpop, Subpopulation *source_subpop, slim_popsize_t p_child_genome_index, slim_objectid_t p_source_subpop_id, slim_popsize_t p_parent_genome_index, const Chromosome &p_chromosome, slim_generation_t p_generation, IndividualSex p_child_sex);
	
	// step forward a generation: remove fixed mutations, then make the children become the parents and update fitnesses
	void SwapGenerations();
	
	// count the total number of times that each Mutation in the registry is referenced by a population, and set total_genome_count_ to the maximum possible number of references (i.e. fixation)
	void TallyMutationReferences(void);
	
	// handle negative fixation (remove from the registry) and positive fixation (convert to Substitution), using reference counts from TallyMutationReferences()
	void RemoveFixedMutations();
	
	// check the registry for any bad entries (i.e. zombies)
	void CheckMutationRegistry(void);
	
	// print all mutations and all genomes to a stream
	void PrintAll(std::ostream &p_out) const;
	
	// print sample of p_sample_size genomes from subpopulation p_subpop_id
	void PrintSample(Subpopulation &p_subpop, slim_popsize_t p_sample_size, IndividualSex p_requested_sex) const;
	
	// print sample of p_sample_size genomes from subpopulation p_subpop_id, using "ms" format
	void PrintSample_ms(Subpopulation &p_subpop, slim_popsize_t p_sample_size, const Chromosome &p_chromosome, IndividualSex p_requested_sex) const;
	
	// remove subpopulations, purge all mutations and substitutions, etc.; called before InitializePopulationFromFile()
	void RemoveAllSubpopulationInfo(void);
	
	// additional methods for SLiMgui, for information-gathering support
#ifdef SLIMGUI
	void SurveyPopulation();
	void AddTallyForMutationTypeAndBinNumber(int p_mutation_type_index, int p_mutation_type_count, slim_generation_t p_bin_number, slim_generation_t **p_buffer, uint32_t *p_bufferBins);
#endif
};


#endif /* defined(__SLiM__population__) */




































































