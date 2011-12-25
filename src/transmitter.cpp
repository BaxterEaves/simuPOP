/**
 *  $File: transmitter.cpp $
 *  $LastChangedDate$
 *  $Rev$
 *
 *  This file is part of simuPOP, a forward-time population genetics
 *  simulation environment. Please visit http://simupop.sourceforge.net
 *  for details.
 *
 *  Copyright (C) 2004 - 2010 Bo Peng (bpeng@mdanderson.org)
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

#include "transmitter.h"

using std::min;
using std::max;

namespace simuPOP {

void GenoTransmitter::initializeIfNeeded(const Individual & ind) const
{
	if (m_lastGenoStru != ind.genoStruIdx()) {
		// this could be the initialize function from a derived class,
		// which will also call GenoTransmitter::initialize(ind)
		initialize(ind);
		m_lastGenoStru = ind.genoStruIdx();
	}
}


void GenoTransmitter::initialize(const Individual & ind) const
{
	m_hasCustomizedChroms = !ind.customizedChroms().empty();
	m_lociToCopy.clear();
	for (size_t ch = 0; ch < ind.numChrom(); ++ch)
		if (ind.chromType(ch) == CUSTOMIZED)
			m_lociToCopy.push_back(0);
		else
			m_lociToCopy.push_back(ind.numLoci(ch));
	m_ploidy = ind.ploidy();
	m_chromIdx = ind.chromIndex();
}


void GenoTransmitter::clearChromosome(const Individual & ind, int ploidy, size_t chrom) const
{
	initializeIfNeeded(ind);

#ifdef BINARYALLELE
	clearGenotype(ind.genoBegin(ploidy) + m_chromIdx[chrom], m_lociToCopy[chrom]);
#else
	DBG_FAILIF(m_chromIdx.empty(), ValueError, "GenoTransmitter is not initialized properly");
#  ifdef MUTANTALLELE
	simuPOP::fill(ind.genoBegin(ploidy) + m_chromIdx[chrom],
		ind.genoBegin(ploidy) + m_chromIdx[chrom + 1], 0);
#  else
	fill(ind.genoBegin(ploidy) + m_chromIdx[chrom],
		ind.genoBegin(ploidy) + m_chromIdx[chrom + 1], 0);
#  endif
#endif
}


void GenoTransmitter::copyChromosome(const Individual & parent, int parPloidy,
                                     Individual & offspring, int ploidy, size_t chrom) const
{
	initializeIfNeeded(offspring);

#ifdef BINARYALLELE
	copyGenotype(parent.genoBegin(parPloidy) + m_chromIdx[chrom],
		offspring.genoBegin(ploidy) + m_chromIdx[chrom],
		m_lociToCopy[chrom]);
#else
	DBG_FAILIF(m_chromIdx.empty(), ValueError,
		"GenoTransmitter is not properly initialized.");
#  ifdef MUTANTALLELE
	simuPOP::copy(parent.genoBegin(parPloidy) + m_chromIdx[chrom],
		parent.genoBegin(parPloidy) + m_chromIdx[chrom + 1],
		offspring.genoBegin(ploidy) + m_chromIdx[chrom]);
#  else
	copy(parent.genoBegin(parPloidy) + m_chromIdx[chrom],
		parent.genoBegin(parPloidy) + m_chromIdx[chrom + 1],
		offspring.genoBegin(ploidy) + m_chromIdx[chrom]);
#  endif
#endif
}


void GenoTransmitter::copyChromosomes(const Individual & parent,
                                      int parPloidy, Individual & offspring, int ploidy) const
{
	initializeIfNeeded(offspring);

	// troublesome ...
	if (m_hasCustomizedChroms) {
		for (size_t ch = 0; ch < parent.numChrom(); ++ch) {
			if (m_lociToCopy[ch] == 0)
				continue;
			GenoIterator par = parent.genoBegin(parPloidy, ch);
			GenoIterator off = offspring.genoBegin(ploidy, ch);

#ifdef BINARYALLELE
			copyGenotype(par, off, m_lociToCopy[ch]);
#else
			GenoIterator par_end = parent.genoEnd(parPloidy, ch);
#  ifdef MUTANTALLELE
			simuPOP::copy(par, par_end, off);
#  else
			copy(par, par_end, off);
#  endif
#endif
		}
	} else {             // easy
#ifdef BINARYALLELE
		copyGenotype(parent.genoBegin(parPloidy), offspring.genoBegin(ploidy),
			offspring.totNumLoci());
#else
#  ifdef MUTANTALLELE
		simuPOP::copy(parent.genoBegin(parPloidy), parent.genoEnd(parPloidy), offspring.genoBegin(ploidy));
#  else
		copy(parent.genoBegin(parPloidy), parent.genoEnd(parPloidy), offspring.genoBegin(ploidy));
#  endif
#endif
	}
}


string CloneGenoTransmitter::describe(bool /* format */) const
{
	return "<simuPOP.CloneGenoTransmitter> clone genotype, sex and information fields of parent to offspring" ;
}


bool CloneGenoTransmitter::applyDuringMating(Population & pop, Population & offPop, RawIndIterator offspring,
                                             Individual * dad, Individual * mom) const
{
	// if offspring does not belong to subPops, do nothing, but does not fail.
	if (!applicableToAllOffspring() && !applicableToOffspring(offPop, offspring))
		return true;

	initializeIfNeeded(*offspring);

	DBG_FAILIF(dad == NULL && mom == NULL, ValueError,
		"Both parents are invalid");

	Individual * parent = mom != NULL ? mom : dad;

	// troublesome ...
	if (!m_chroms.allAvail()) {
		const vectoru chroms = m_chroms.elems();
		for (size_t p = 0; p != m_ploidy; ++p) {
			for (size_t i = 0; i < chroms.size(); ++i) {
				size_t ch = chroms[i];
				GenoIterator par = parent->genoBegin(p, ch);
				GenoIterator off = offspring->genoBegin(p, ch);
#ifdef BINARYALLELE
				copyGenotype(par, off, offspring->numLoci(ch));
#else
				GenoIterator par_end = parent->genoEnd(p, ch);
#  ifdef MUTANTALLELE
				simuPOP::copy(par, par_end, off);
#  else
				copy(par, par_end, off);
#  endif
#endif
			}
		}
	} else if (m_hasCustomizedChroms) {
		for (size_t p = 0; p != m_ploidy; ++p) {
			for (size_t ch = 0; ch < pop.numChrom(); ++ch) {
				if (m_lociToCopy[ch] == 0)
					continue;
				GenoIterator par = parent->genoBegin(p, ch);
				GenoIterator off = offspring->genoBegin(p, ch);
#ifdef BINARYALLELE
				copyGenotype(par, off, m_lociToCopy[ch]);
#else
				GenoIterator par_end = parent->genoEnd(p, ch);
#  ifdef MUTANTALLELE
				simuPOP::copy(par, par_end, off);
#  else
				copy(par, par_end, off);
#  endif
#endif
			}
		}
	} else {             // easy
#ifdef BINARYALLELE
		copyGenotype(parent->genoBegin(), offspring->genoBegin(),
			offspring->genoSize());
#else
#  ifdef MUTANTALLELE
                simuPOP::copy(parent->genoBegin(), parent->genoEnd(), offspring->genoBegin());
#  else
                copy(parent->genoBegin(), parent->genoEnd(), offspring->genoBegin());
#  endif
#endif
        }
	// for clone transmitter, sex is also transmitted
	offspring->setSex(parent->sex());
	if (infoFields().allAvail()) {
		for (size_t i = 0; i < parent->infoFields().size(); ++i)
			offspring->setInfo(parent->info(i), i);
	} else {
		for (size_t i = 0; i < infoSize(); ++i) {
			size_t idx = parent->infoIdx(infoField(i));
			offspring->setInfo(parent->info(idx), idx);
		}
	}
	return true;
}


void MendelianGenoTransmitter::initialize(const Individual & ind) const
{
	GenoTransmitter::initialize(ind);
	m_chromX = ind.chromX();
	m_chromY = ind.chromY();
	m_numChrom = ind.numChrom();
}


void MendelianGenoTransmitter::transmitGenotype(const Individual & parent,
                                                Individual & offspring, int ploidy) const
{
	initializeIfNeeded(offspring);

	// current parental ploidy (copy from which chromosome copy)
	int parPloidy = 0;

#ifdef BINARYALLELE
	// for the simple case, use faster algorithm
	if (m_chromX < 0 && m_chromY < 0 && !m_hasCustomizedChroms) {
		// pointer to parental, and offspring chromosome copies
		GenoIterator par[2];
		GenoIterator off; 
		//
		par[0] = parent.genoBegin(0);
		par[1] = parent.genoBegin(1);
		off = offspring.genoBegin(ploidy);
		//
		//
		// 1. try to copy in blocks,
		// 2. if two chromosomes can be copied together, copy together
		// 3. if length is short, using the old method.
		//
		size_t parBegin = 0;
		size_t parEnd = 0;
		// first chromosome
		parPloidy = getRNG().randBit();
		//
		int nextParPloidy = 0;
		bool copyPar;
		for (size_t ch = 0; ch < m_numChrom; ++ch) {
			// if it is the last chromosome, copy anyway
			if (ch == m_numChrom - 1)
				copyPar = true;
			else {                                                                 // is there a different chromosome?
				nextParPloidy = getRNG().randBit();
				copyPar = parPloidy != nextParPloidy;
			}
			if (copyPar) {
				// end of this chromosome, is the beginning of the next
				parEnd = m_chromIdx[ch + 1];
				size_t length = parEnd - parBegin;
				//
				// the easiest case, try to get some speed up...
				if (length == 1)
					off[parBegin] = par[parPloidy][parBegin];
				else
					copyGenotype(par[parPloidy] + parBegin, off + parBegin, length);
				//
				if (ch != m_numChrom - 1)
					parPloidy = nextParPloidy;
				parBegin = parEnd;
			}
		}
		return;
	}
#endif
	//
	for (int ch = 0; static_cast<size_t>(ch) < m_numChrom; ++ch) {
		// customized chromosome?
		if (m_lociToCopy[ch] == 0)
			continue;
		if ((ploidy == 0 && ch == m_chromY) ||   // maternal, Y chromosome
		    (ploidy == 1 &&
		     ((ch == m_chromX && offspring.sex() == MALE) ||
		      (ch == m_chromY && offspring.sex() == FEMALE)))) {
			clearChromosome(offspring, ploidy, ch);
			continue;
		}
		if (ploidy == 1 && ch == m_chromX)
			parPloidy = 0;
		else if (ploidy == 1 && ch == m_chromY)
			parPloidy = 1;          // copy chrom Y from second ploidy
		else
			parPloidy = getRNG().randBit();
		//
		copyChromosome(parent, parPloidy, offspring, ploidy, ch);
	}
}


bool MendelianGenoTransmitter::applyDuringMating(Population & /* pop */,
                                                 Population & offPop, RawIndIterator offspring,
                                                 Individual * dad, Individual * mom) const
{
	// if offspring does not belong to subPops, do nothing, but does not fail.
	if (!applicableToAllOffspring() && !applicableToOffspring(offPop, offspring))
		return true;
	DBG_FAILIF(mom == NULL || dad == NULL, ValueError,
		"Mendelian offspring generator requires two valid parents");

	initializeIfNeeded(*offspring);
	// the next two functions.
	transmitGenotype(*mom, *offspring, 0);
	transmitGenotype(*dad, *offspring, 1);
	return true;
}


bool SelfingGenoTransmitter::applyDuringMating(Population & /* pop */, Population & offPop, RawIndIterator offspring,
                                               Individual * dad, Individual * mom) const
{
	// if offspring does not belong to subPops, do nothing, but does not fail.
	if (!applicableToAllOffspring() && !applicableToOffspring(offPop, offspring))
		return true;
	//
	DBG_FAILIF(mom == NULL && dad == NULL, ValueError,
		"Selfing genotype transmitter requires at least one valid parents");

	// call MendelianGenoTransmitter::initialize if needed.
	Individual * parent = mom != NULL ? mom : dad;

	initializeIfNeeded(*offspring);
	// use the same parent to produce two copies of chromosomes
	transmitGenotype(*parent, *offspring, 0);
	transmitGenotype(*parent, *offspring, 1);
	return true;
}


void HaplodiploidGenoTransmitter::initialize(const Individual & ind) const
{
	DBG_FAILIF(ind.chromX() >= 0 || ind.chromY() >= 0, ValueError,
		"Haplodiploid indulations do not use sex chromosomes");
	MendelianGenoTransmitter::initialize(ind);
}


bool HaplodiploidGenoTransmitter::applyDuringMating(Population & /* pop */,
                                                    Population & offPop, RawIndIterator offspring,
                                                    Individual * dad, Individual * mom) const
{
	// if offspring does not belong to subPops, do nothing, but does not fail.
	if (!applicableToAllOffspring() && !applicableToOffspring(offPop, offspring))
		return true;
	DBG_FAILIF(dad == NULL || mom == NULL, ValueError,
		"haplodiploid offspring generator: one of the parents is invalid.");

	initializeIfNeeded(*offspring);
	// mom generate the first...
	transmitGenotype(*mom, *offspring, 0);

	if (offspring->sex() == FEMALE)
		copyChromosomes(*dad, 0, *offspring, 1);
	return true;
}


void MitochondrialGenoTransmitter::initialize(const Individual & ind) const
{
	GenoTransmitter::initialize(ind);
	if (m_chroms.allAvail()) {
		for (size_t ch = 0; ch < ind.numChrom(); ++ch)
			if (ind.chromType(ch) == CUSTOMIZED)
				m_mitoChroms.push_back(ch);
	} else
		m_mitoChroms = m_chroms.elems();
	DBG_DO(DBG_TRANSMITTER, cerr << "Mitochondrial chromosomes " << m_mitoChroms << endl);
	if (m_mitoChroms.empty())
		return;

	m_numLoci = ind.numLoci(m_mitoChroms[0]);

#ifndef OPTIMIZED
	for (size_t ch = 1; ch < m_mitoChroms.size(); ++ch) {
		DBG_FAILIF(ind.numLoci(m_mitoChroms[ch]) != m_numLoci, ValueError,
			"All mitochondrial chromosomes should have the same number of loci");
	}
#endif
}


bool MitochondrialGenoTransmitter::applyDuringMating(Population & pop, Population & offPop, RawIndIterator offspring,
                                                     Individual * /* dad */, Individual * mom) const
{
	// if offspring does not belong to subPops, do nothing, but does not fail.
	if (!applicableToAllOffspring() && !applicableToOffspring(offPop, offspring))
		return true;
	initializeIfNeeded(*offspring);

	(void)mom;  // avoid a warning message of unused varible in optimized modules
	DBG_FAILIF(mom == NULL, ValueError,
		"MitochondrialGenoTransmitter requires valid female parent.");

	if (m_numLoci == 0)
		return true;

	size_t pldy = pop.ploidy();
	//
	vectoru::iterator it = m_mitoChroms.begin();
	vectoru::iterator it_end = m_mitoChroms.end();
	for (; it != it_end; ++it) {
		size_t src = getRNG().randInt(static_cast<ULONG>(m_mitoChroms.size()));
		GenoIterator par = mom->genoBegin(0, m_mitoChroms[src]);
		GenoIterator off = offspring->genoBegin(0, *it);
#ifdef BINARYALLELE
		copyGenotype(par, off, m_numLoci);
#else
		GenoIterator par_end = mom->genoEnd(0, m_mitoChroms[src]);
#  ifdef MUTANTALLELE
		simuPOP::copy(par, par_end, off);
#  else
		copy(par, par_end, off);
#  endif
#endif
		for (size_t p = 1; p < pldy; ++p)
			clearChromosome(*offspring, 1, static_cast<int>(*it));
	}

	return true;
}


Recombinator::Recombinator(const floatList & rates, double intensity,
	const lociList & loci, const floatList & convMode,
	const stringFunc & output, int begin, int end, int step, const intList & at,
	const intList & reps, const subPopList & subPops, const stringList & infoFields)
	:
	GenoTransmitter(output, begin, end, step, at, reps, subPops, infoFields),
	m_intensity(intensity), m_rates(rates.elems()), m_loci(loci),
	m_recBeforeLoci(0), m_convMode(convMode.elems()), m_chromX(-1), m_chromY(-1),
	m_customizedBegin(-1), m_customizedEnd(-1), m_algorithm(0), m_debugOutput(NULL),
#ifdef _OPENMP
	m_bt(numThreads(), getRNG())
#else
	m_bt(getRNG())
#endif
{

	DBG_FAILIF(m_convMode.empty(), ValueError,
		"Please specify a conversion mode");

	int mode = static_cast<int>(m_convMode[0]);
	(void)mode;  // avoid a warning.
	DBG_FAILIF(mode != NO_CONVERSION && m_convMode.size() != 3,
		ValueError, "Two parameters are required for a non-NoConversion conversion mode");

	DBG_FAILIF(mode != NO_CONVERSION && (fcmp_lt(m_convMode[1], 0) || fcmp_gt(m_convMode[1], 1)),
		ValueError, "Conversion probability should be between 0 and 1");
};


string Recombinator::describe(bool /* format */) const
{
	string desc = "<simuPOP.Recombinator> genetic recombination.";

	return desc;
}


size_t Recombinator::markersConverted(size_t index, const Individual & ind) const
{
	int mode = static_cast<int>(m_convMode[0]);

	// IMPORTANT: if conversion length reaches end of chromosome
	// this is an recombination! Otherwise, conversion will
	// interfere with free crossover between chromosomes
	if (mode == NUM_MARKERS || mode == GEOMETRIC_DISTRIBUTION) {
		size_t num = 0;
		if (mode == NUM_MARKERS)
			num = static_cast<int>(m_convMode[2]);
		else
			num = getRNG().randGeometric(m_convMode[2]);

		// if conversion reaches end of chromosome, it is an recombination event
		if (num == 0 || num >= ind.lociLeft(index))
			return 0;
		else
			return num;
	} else {
		double len = 0;
		if (mode == TRACT_LENGTH)
			len = m_convMode[2];
		else
			len = getRNG().randExponential(len);
		//
		// recombination starts 'before' index so we assume that it happens
		// randomly (uniformly) between this and previous marker
		if (index > 0)
			len -= getRNG().randUniform() * ind.lociDist(index - 1, index);
		if (len <= 0. || len >= ind.distLeft(index))
			return 0;
		else
			return ind.lociCovered(index, len);
	}
}


void Recombinator::initialize(const Individual & ind) const
{
	GenoTransmitter::initialize(ind);

	m_chromX = ind.chromX();
	m_chromY = ind.chromY();
	if (!ind.customizedChroms().empty()) {
		m_customizedBegin = static_cast<int>(ind.chromBegin(ind.customizedChroms()[0]));
		m_customizedEnd = static_cast<int>(ind.chromEnd(ind.customizedChroms().back()));
	}
	// prepare m_bt
	vectorf vecP;
	//
	const vectoru & loci = m_loci.elems(&ind);

	DBG_FAILIF(m_intensity < 0 && m_rates.empty(), ValueError,
		"You should specify m_intensity, or m_rates "
		"(a number or a sequence of recombination m_ratess.)");

	DBG_FAILIF(m_rates.size() > 1 && loci.empty(), ValueError,
		"When more than one m_ratess are given, loci should be"
		" explicitly specified.");

	DBG_FAILIF(m_rates.size() > 1 && m_rates.size() != loci.size(),
		ValueError, "If both rates and loci are specified, "
		            "they should have the same length.");

	bool useLociDist = m_rates.empty();

	m_recBeforeLoci.clear();
	vecP.clear();
	for (size_t ch = 0; ch < ind.numChrom(); ++ch) {
		size_t chBegin = ind.chromBegin(ch);
		size_t chEnd = ind.chromEnd(ch);

		if (chBegin == chEnd)
			continue;

		if (ind.chromType(ch) == CUSTOMIZED) {
			// recombine before customized chromosome.
			if (ind.numChrom() != ch + 1 && ind.chromType(ch + 1) != CUSTOMIZED) {
				m_recBeforeLoci.push_back(chEnd);
				vecP.push_back(0.5);
			}
			continue;
		}

		if (m_loci.allAvail()) {
			// get loci distance * m_rates and then recombinant points
			for (size_t loc = chBegin; loc < chEnd - 1; ++loc) {
				m_recBeforeLoci.push_back(loc + 1);
				double r = useLociDist ? ((ind.locusPos(loc + 1) - ind.locusPos(loc)) * m_intensity) : m_rates[0];

				DBG_WARNIF(fcmp_gt(r, 0.5),
					"Recombination m_rates after marker " + toStr(loc) + " is out of range ("
					+ toStr(r) + " ) so it is set to 0.5. This may happen \n"
					             "when you use recombination m_intensity instead of m_rates, and your loci \n"
					             "distance is too high.)");
				vecP.push_back(min(0.5, r));
			}
		} else {
			DBG_FAILIF(m_rates.size() > 1 && m_rates.size() != loci.size(), SystemError,
				"If an array is given, rates and loci should have the same length");

			// get loci distance * m_rates and then recombinant points
			for (size_t loc = chBegin; loc < chEnd - 1; ++loc) {
				// if this locus will be recombined.
				vectoru::const_iterator pos = find(loci.begin(), loci.end(), loc);
				if (pos != loci.end()) {
					double r = 0;
					if (useLociDist)
						r = m_intensity > 0 ? ((ind.locusPos(loc + 1) - ind.locusPos(loc)) * m_intensity) : r;
					else if (m_rates.size() == 1 && !useLociDist)
						r = max(m_rates[0], 0.);
					else
						r = m_rates[pos - loci.begin()];
					m_recBeforeLoci.push_back(loc + 1);
					vecP.push_back(r);

					DBG_ASSERT(fcmp_ge(vecP[vecP.size() - 1], 0) && fcmp_le(vecP[vecP.size() - 1], 1),
						ValueError,
						"Recombination m_rates should be in [0,1]. (Maybe your loci distance is too high.)");
				}
			}
		}
		// after each chromosome ...
		m_recBeforeLoci.push_back(chEnd);
		vecP.push_back(0.5);
	}
	DBG_DO(DBG_TRANSMITTER, cerr	<< "Specify after Loci. With m_ratess "
		                            << vecP << " before " << m_recBeforeLoci << endl);

	DBG_FAILIF(vecP.empty(), ValueError, "No non-empty chromosome.");

	DBG_ASSERT(vecP.size() == m_recBeforeLoci.size(), SystemError,
		"Rate and before loci should have the same length.");

	DBG_FAILIF(ind.chromType(ind.numChrom() - 1) != CUSTOMIZED && !m_recBeforeLoci.empty() && m_recBeforeLoci.back() != ind.totNumLoci(),
		SystemError,
		"The last beforeLoci elem should be total number of loci. (If the last chromsome is not customized");

	DBG_ASSERT(vecP.back() == .5, SystemError,
		"The last elem of m_rates should be half.");

	// if the operator is called directly, there is no way to know population size so we
	// a variable to tell it.
#ifdef _OPENMP
	for (size_t i = 0; i < numThreads(); i++)
		m_bt[i].setParameter(vecP, 0 /* obsolete m_intendedSize */);
#else

	m_bt.setParameter(vecP, 0 /* obsolete m_intendedSize */);
#endif
	// choose an algorithm
	// if recombinations are dense. use the first algorithm
	// For example 10 chromoes, regular 0.5*10=5
	// if there are high recombination on chromosomes, ....
	//
	// In addition, the second algorithm is really difficult in the
	// handling of sex chromosomes etc.
	if (std::accumulate(vecP.begin(), vecP.end(), 0.) > ind.numChrom()
	    || m_chromX > 0 || m_customizedBegin > 0)
		m_algorithm = 0;
	else
		m_algorithm = 1;
	DBG_DO(DBG_TRANSMITTER, cerr << "Algorithm " << m_algorithm << " is being used " << endl);
}


void Recombinator::transmitGenotype(const Individual & parent,
                                    Individual & offspring, int ploidy) const
{
	initializeIfNeeded(offspring);

	//Bernullitrial for each thread
#ifdef _OPENMP
	Bernullitrials & bt = m_bt[omp_get_thread_num()];
#else
	Bernullitrials & bt = m_bt;
#endif

	// use which copy of chromosome
	GenoIterator cp[2], off;

	cp[0] = parent.genoBegin(0);
	cp[1] = parent.genoBegin(1);
	off = offspring.genoBegin(ploidy);

	// handling of sex chromosomes, by specifying chromsome
	// ranges with specified ploidy.
	int ignoreBegin = -1;
	int ignoreEnd = -1;
	int forceFirstBegin = -1;
	int forceFirstEnd = -1;
	int forceSecondBegin = -1;
	int forceSecondEnd = -1;

	// from maternal, ignore chromosome Y
	if (ploidy == 0 && m_chromY > 0) {
		ignoreBegin = static_cast<int>(parent.chromBegin(m_chromY));
		ignoreEnd = static_cast<int>(parent.chromEnd(m_chromY));
	} else if (ploidy == 1 && m_chromX > 0) {
		if (offspring.sex() == MALE) {
			ignoreBegin = static_cast<int>(parent.chromBegin(m_chromX));
			ignoreEnd = static_cast<int>(parent.chromEnd(m_chromX));
			forceSecondBegin = static_cast<int>(parent.chromBegin(m_chromY));
			forceSecondEnd = static_cast<int>(parent.chromEnd(m_chromY));
		} else {
			ignoreBegin = static_cast<int>(parent.chromBegin(m_chromY));
			ignoreEnd = static_cast<int>(parent.chromEnd(m_chromY));
			forceFirstBegin = static_cast<int>(parent.chromBegin(m_chromX));
			forceFirstEnd = static_cast<int>(parent.chromEnd(m_chromX));
		}
	}
	// get a new set of values.
	// const BoolResults& bs = bt.trial();
	bt.trial();
	int curCp = bt.trialSucc(m_recBeforeLoci.size() - 1) ? 0 : 1;
	curCp = forceFirstBegin == 0 ? 0 : (forceSecondBegin == 0 ? 1 : curCp);

	if (m_debugOutput)
		*m_debugOutput << offspring.intInfo(infoField(0)) << ' ' << parent.intInfo(infoField(0)) << ' ' << curCp;

	// the last one does not count, because it determines
	// the initial copy of paternal chromosome
	bt.setTrialSucc(m_recBeforeLoci.size() - 1, false);

	// algorithm one:
	//
	//  gt: index on chromosomes
	//  gtEnd: total number of loci
	//
	//  at each locus, check if recombine after it, if so
	//  recombine.
	bool withConversion = static_cast<int>(m_convMode[0]) != NO_CONVERSION
	                      && m_convMode[1] > 0.;
	if (m_algorithm == 0) {
		// negative means no conversion is pending.
		ssize_t convCount = -1;
		size_t gtEnd = m_recBeforeLoci.back();
		for (size_t gt = 0, bl = 0; gt < gtEnd; ++gt, --convCount) {
			// do not copy genotype in the ignored region.
			if ((ignoreBegin < 0 || gt < static_cast<size_t>(ignoreBegin) || gt >= static_cast<size_t>(ignoreEnd)) &&
			    (m_customizedBegin < 0 || gt < static_cast<size_t>(m_customizedBegin) || gt >= static_cast<size_t>(m_customizedEnd)))
				// copy
				off[gt] = cp[curCp][gt];
			// look ahead
			if (convCount == 0) {             // conversion ...
				if (forceFirstBegin > 0 && gt + 1 >= static_cast<size_t>(forceFirstBegin)
				    && gt + 1 < static_cast<size_t>(forceFirstEnd)) {
					if (curCp != 0 && m_debugOutput)
						*m_debugOutput << ' ' << gt;
					curCp = 0;
				} else if (forceSecondBegin > 0 && gt + 1 >= static_cast<size_t>(forceSecondBegin)
				           && gt + 1 < static_cast<size_t>(forceSecondEnd)) {
					if (curCp != 1 && m_debugOutput)
						*m_debugOutput << ' ' << gt;
					curCp = 1;
				} else {
					curCp = (curCp + 1) % 2;
					if (m_debugOutput)
						*m_debugOutput << ' ' << gt;
				}
				//
				// no pending conversion
				convCount = -1;
			}
			if (gt + 1 == m_recBeforeLoci[bl]) {
				DBG_DO(DBG_TRANSMITTER, cerr << gt << " " << m_recBeforeLoci[bl] << ", ");
				if (forceFirstBegin >= 0 && gt + 1 >= static_cast<size_t>(forceFirstBegin)
				    && gt + 1 < static_cast<size_t>(forceFirstEnd)) {
					if (curCp != 0 && m_debugOutput)
						*m_debugOutput << ' ' << gt;
					curCp = 0;
					convCount = -1;
				} else if (forceSecondBegin >= 0 && gt + 1 >= static_cast<size_t>(forceSecondBegin)
				           && gt + 1 < static_cast<size_t>(forceSecondEnd)) {
					if (curCp != 1 && m_debugOutput)
						*m_debugOutput << ' ' << gt;
					curCp = 1;
					convCount = -1;
				} else if (convCount < 0 && bt.trialSucc(bl)) {
					// recombination (if convCount == 0, a conversion event is ending)
					curCp = (curCp + 1) % 2;
					if (m_debugOutput)
						*m_debugOutput << ' ' << gt;
					// if conversion happens
					if (withConversion &&
					    parent.lociLeft(gt) != 1 &&             // can not be at the end of a chromosome
					    (m_convMode[1] == 1. || getRNG().randUniform() < m_convMode[1])) {
						// convCount will be decreased, until reconversion completes
						// or another recombination happens
						convCount = markersConverted(gt + 1, parent);
					} else
						// another recombination stops the previous conversion
						convCount = -1;
				}
				++bl;
			}
		}
	} else {
#ifndef BINARYALLELE
		size_t gt = 0, gtEnd = 0;
		size_t pos = bt.probFirstSucc();
		// if there is some recombination
		ssize_t convCount = -1;
		size_t convEnd;
		if (pos != Bernullitrials::npos) {
			// first piece
			for (; gt < m_recBeforeLoci[pos]; ++gt)
				off[gt] = cp[curCp][gt];
			curCp = (curCp + 1) % 2;
			if (m_debugOutput)
				*m_debugOutput << ' ' << gt - 1;
			//
			if (withConversion &&
			    parent.lociLeft(gt - 1) != 1 &&             // can not be at the end of a chromosome
			    (m_convMode[1] == 1. || getRNG().randUniform() < m_convMode[1])) {
				convCount = markersConverted(gt, parent);
			}
			// next recombination point...
			while ((pos = bt.probNextSucc(pos)) != Bernullitrials::npos) {
				// copy from last to this recombination point, but
				// there might be a conversion event in between
				gtEnd = m_recBeforeLoci[pos];
				if (convCount > 0) {
					convEnd = gt + convCount;
					if (convEnd < gtEnd) {
						for (; gt < convEnd; ++gt)
							off[gt] = cp[curCp][gt];
						curCp = (curCp + 1) % 2;
						if (m_debugOutput)
							*m_debugOutput << ' ' << gt - 1;
					}
					// no pending conversion
					convCount = -1;
				}
				// copy from the end of conversion to this recombination point
				for (; gt < gtEnd; ++gt)
					off[gt] = cp[curCp][gt];
				curCp = (curCp + 1) % 2;
				if (m_debugOutput)
					*m_debugOutput << ' ' << gt - 1;
				//
				// conversion event for this recombination event
				if (withConversion &&
				    parent.lociLeft(gt - 1) != 1 &&             // can not be at the end of a chromosome
				    (m_convMode[1] == 1. || getRNG().randUniform() < m_convMode[1])) {
					// convCount will be decreased, until reconversion completes
					// or another recombination happens
					convCount = markersConverted(gt, parent);
				}
			}
		}
		gtEnd = m_recBeforeLoci.back();
		// copy the last piece
		if (convCount > 0) {
			convEnd = gt + convCount;
			if (convEnd < gtEnd) {
				for (; gt < convEnd; ++gt)
					off[gt] = cp[curCp][gt];
				curCp = (curCp + 1) % 2;
				if (m_debugOutput)
					*m_debugOutput << ' ' << gt - 1;
			}
		}
		for (; gt < gtEnd; ++gt)
			off[gt] = cp[curCp][gt];
#else
		size_t gt = 0, gtEnd = 0;
		size_t pos = bt.probFirstSucc();
		// if there is some recombination
		ssize_t convCount = -1;
		size_t convEnd;
		if (pos != Bernullitrials::npos) {
			// first piece
			gtEnd = m_recBeforeLoci[pos];
			copyGenotype(cp[curCp] + gt, off + gt, m_recBeforeLoci[pos] - gt);
			gt = gtEnd;
			curCp = (curCp + 1) % 2;
			if (m_debugOutput)
				*m_debugOutput << ' ' << gt - 1;
			if (withConversion &&
			    parent.lociLeft(gt - 1) != 1 &&             // can not be at the end of a chromosome
			    (m_convMode[1] == 1. || getRNG().randUniform() < m_convMode[1])) {
				convCount = markersConverted(gt, parent);
			}
			// next recombination point...
			while ((pos = bt.probNextSucc(pos)) != Bernullitrials::npos) {
				gtEnd = m_recBeforeLoci[pos];
				if (convCount > 0) {
					convEnd = gt + convCount;
					if (convEnd < gtEnd) {
						copyGenotype(cp[curCp] + gt, off + gt, convCount);
						gt = convEnd;
						curCp = (curCp + 1) % 2;
						if (m_debugOutput)
							*m_debugOutput << ' ' << gt - 1;
					}
					// no pending conversion
					convCount = -1;
				}
				// copy from the end of conversion to the next recombination point
				copyGenotype(cp[curCp] + gt, off + gt, m_recBeforeLoci[pos] - gt);
				gt = gtEnd;
				curCp = (curCp + 1) % 2;
				if (m_debugOutput)
					*m_debugOutput << ' ' << gt - 1;
				// conversion event for this recombination event
				if (withConversion &&
				    parent.lociLeft(gt - 1) != 1 &&             // can not be at the end of a chromosome
				    (m_convMode[1] == 1. || getRNG().randUniform() < m_convMode[1])) {
					// convCount will be decreased, until reconversion completes
					// or another recombination happens
					convCount = markersConverted(gt, parent);
				}
			}
		}
		gtEnd = m_recBeforeLoci.back();
		// copy the last piece
		if (convCount > 0) {
			convEnd = gt + convCount;
			if (convEnd < gtEnd) {
				copyGenotype(cp[curCp] + gt, off + gt, convCount);
				gt = convEnd;
				curCp = (curCp + 1) % 2;
				if (m_debugOutput)
					*m_debugOutput << ' ' << gt - 1;
			}
		}
		copyGenotype(cp[curCp] + gt, off + gt, gtEnd - gt);
#endif
	}
	if (m_debugOutput)
		*m_debugOutput << '\n';
	// handle special chromosomes
	if (m_chromX > 0) {
		if (offspring.sex() == FEMALE) {
			clearChromosome(offspring, 0, m_chromY);
			clearChromosome(offspring, 1, m_chromY);
		} else {
			clearChromosome(offspring, 0, m_chromY);
			clearChromosome(offspring, 1, m_chromX);
		}
	}
}


bool Recombinator::applyDuringMating(Population & pop, Population & offPop, RawIndIterator offspring,
                                     Individual * dad, Individual * mom) const
{
	// if offspring does not belong to subPops, do nothing, but does not fail.
	if (!applicableToAllOffspring() && !applicableToOffspring(offPop, offspring))
		return true;

	initializeIfNeeded(*offspring);

	DBG_FAILIF(dad == NULL && mom == NULL,
		ValueError, "None of the parents is invalid.");

	DBG_FAILIF(m_recBeforeLoci.empty(), ValueError,
		"Uninitialized Recombinator");

	if (infoSize() == 1 && !noOutput())
		m_debugOutput = &getOstream(pop.dict());
	else
		m_debugOutput = NULL;
	transmitGenotype(*(mom ? mom : dad), *offspring, 0);
	transmitGenotype(*(dad ? dad : mom), *offspring, 1);

	if (m_debugOutput)
		closeOstream();
	return true;
}


}
