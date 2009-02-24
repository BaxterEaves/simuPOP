/**
 *  $File: mutator.cpp $
 *  $LastChangedDate$
 *  $Rev$
 *
 *  This file is part of simuPOP, a forward-time population genetics
 *  simulation environment. Please visit http://simupop.sourceforge.net
 *  for details.
 *
 *  Copyright (C) 2004 - 2009 Bo Peng (bpeng@mdanderson.org)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "mutator.h"

namespace simuPOP {

void mutator::initialize(population & pop)
{
#ifndef BINARYALLELE
	if (m_maxAllele == 0)
		m_maxAllele = MaxAllele();
	else if (m_maxAllele > 0 && m_maxAllele > MaxAllele() )
		throw ValueError("maxAllele exceeds population max allele.");
#endif

	DBG_DO(DBG_MUTATOR, cout << "initialize mutator" << endl);

	// deal with applicable loci
	if (m_loci.empty() ) {
		// all loci
		m_loci.resize(pop.totNumLoci() );
		for (UINT i = 0, iEnd = pop.totNumLoci(); i < iEnd;  ++i)
			m_loci[i] = i;
	}
#ifndef OPTIMIZED
	else {
		for (UINT i = 0; i < m_loci.size(); ++i) {
			if (m_loci[i] >= pop.totNumLoci())
				throw IndexError("Given loci is out of range");
		}
	}
#endif

	// all use the same rate
	if (m_rate.size() < m_loci.size() ) {
		m_rate.resize(m_loci.size());
		fill(m_rate.begin() + 1, m_rate.end(), m_rate[0]);
	}

	m_bt.setParameter(m_rate, pop.ploidy() * pop.popSize());

#ifndef OPTIMIZED
	for (size_t i = 0; i < m_rate.size(); ++i)
		if (fcmp_lt(m_rate[i], 0.) || fcmp_gt(m_rate[i], 1.) )
			throw ValueError("Migration rate should be between [0,1], given " + toStr(m_rate[i]));
#endif
	if (pop.totNumLoci() != m_mutCount.size())
		m_mutCount.resize(pop.totNumLoci(), 0);
	m_initialized = true;
}


bool mutator::apply(population & pop)
{
	if (!m_initialized || m_bt.trialSize() != pop.ploidy() * pop.popSize()) {
		initialize(pop);
		DBG_DO(DBG_MUTATOR, cout << "Reinitialize mutator at loci" << m_loci <<
			" at rate " << m_rate << endl);
	}

	DBG_DO(DBG_MUTATOR, cout << "Mutate replicate " << pop.rep() << endl);

	m_bt.doTrial();

	// mutate each mutable locus
	for (size_t i = 0, iEnd = m_loci.size(); i < iEnd; ++i) {
		int locus = m_loci[i];
		DBG_DO(DBG_MUTATOR, cout << "Mutate at locus " << locus << endl);
		size_t pos = m_bt.trialFirstSucc(i);
		if (pos != BernulliTrials::npos) {
			do {
#ifndef OPTIMIZED
				// alleleBegin is *not* ordered, so the mutation is random
				AlleleRef ptr = *(pop.alleleBegin(locus) + pos);
				DBG_DO(DBG_MUTATOR, cout << "Mutate locus " << locus
					                     << " of individual " << (pos / pop.ploidy()) << " from " << int(ptr));
				mutate(ptr);
				DBG_DO(DBG_MUTATOR, cout << " to " << int(ptr) << endl);
#else
				mutate(*(pop.alleleBegin(locus) + pos));
#endif
				m_mutCount[ locus ]++;
			} while ( (pos = m_bt.trialNextSucc(i, pos)) != BernulliTrials::npos);
		}                                                                                           // succ.any
	}                                                                                               // each applicable loci

	return true;
}


// mutate to a state other than current state with equal probability
void kamMutator::mutate(AlleleRef allele)
{
#ifdef BINARYALLELE
	allele = !allele;
#else
	Allele new_allele = static_cast<Allele>(rng().randInt(maxAllele()));
	if (new_allele >= allele)
		allele = new_allele + 1;
	else
		allele = new_allele;
#endif
}


void gsmMutator::mutate(AlleleRef allele)
{
	int step;

	if (!m_func.isValid())  // use a geometric distribution.
		step = rng().randGeometric(m_p);
	else
		step = m_func(PyObj_As_Int, "()");

	DBG_DO(DBG_MUTATOR, cout << "step is " << step << endl);

	// increase
	if (rng().randUniform01() < m_incProb) {
#ifdef BINARYALLELE
		allele = 1;
#else
		if (static_cast<UINT>(allele + step) < this->maxAllele() )
			AlleleAdd(allele, step);
		else
			allele = static_cast<Allele>(this->maxAllele());
#endif
	}
	// decrease
	else {
#ifdef BINARYALLELE
		allele = 0;
#else
		if (allele > step)
			AlleleMinus(allele, step);
		else
			allele = 0;
#endif
	}
}


// mutate according to the mixed model
void pyMutator::mutate(AlleleRef allele)
{
	int resInt = m_func(PyObj_As_Int, "(i)", static_cast<int>(allele));

#ifdef BINARYALLELE
	DBG_ASSERT(resInt == 0 || resInt == 1, ValueError,
		"Can only mutate to 0 or 1 in binary mode.");
	allele = resInt != 0;
#else
	DBG_ASSERT(static_cast<unsigned>(resInt) <= ModuleMaxAllele, ValueError,
		"Mutated to an allele greater than maximum allowed allele value");
	allele = static_cast<Allele>(resInt);
#endif
}


bool pointMutator::apply(population & pop)
{
	m_mutCount.resize(pop.totNumLoci(), 0);
	// mutate each mutable locus
	for (size_t i = 0, iEnd = m_loci.size(); i < iEnd; ++i) {
		for (vectorlu::iterator ind = m_inds.begin();
			ind != m_inds.end(); ++ind) {
			for (size_t p = 0; p < m_atPloidy.size(); ++p) {
				m_mutCount[m_loci[i]]++;
				*(pop.ind(*ind).genoBegin(m_atPloidy[p]) + m_loci[i]) = m_toAllele;
				DBG_DO(DBG_MUTATOR, cout << "Mutate locus " << m_loci[i] <<
					" to allele " << toStr(m_toAllele) << " at generation " << pop.gen() << endl);
			}
		}
	}                                                                                 // each applicable loci

	return true;
}
}
