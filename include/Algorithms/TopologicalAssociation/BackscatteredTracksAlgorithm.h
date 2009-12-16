/**
 *  @file   PandoraPFANew/include/Algorithms/TopologicalAssociation/BackscatteredTracksAlgorithm.h
 * 
 *  @brief  Header file for the backscattered tracks algorithm class.
 * 
 *  $Log: $
 */
#ifndef BACKSCATTERED_TRACKS_ALGORITHM_H
#define BACKSCATTERED_TRACKS_ALGORITHM_H 1

#include "Algorithms/Algorithm.h"

/**
 *  @brief  BackscatteredTracksAlgorithm class
 */
class BackscatteredTracksAlgorithm : public pandora::Algorithm
{
public:
    /**
     *  @brief  Factory class for instantiating algorithm
     */
    class Factory : public pandora::AlgorithmFactory
    {
    public:
        Algorithm *CreateAlgorithm() const;
    };

private:
    StatusCode Run();
    StatusCode ReadSettings(const TiXmlHandle xmlHandle);

    float           m_canMergeMinMipFraction;           ///< The min mip fraction for clusters (flagged as photons) to be merged
    float           m_canMergeMaxRms;                   ///< The max all hit fit rms for clusters (flagged as photons) to be merged

    unsigned int    m_minCaloHitsPerCluster;            ///< The min number of calo hits for cluster to be used as a daughter cluster
    float           m_fitToAllHitsRmsCut;               ///< The max rms value (for the fit to all hits) to use cluster as a daughter

    unsigned int    m_nOuterFitExclusionLayers;         ///< The number of outer layers to exclude from the daughter cluster fit
    unsigned int    m_nFitProjectionLayers;             ///< The number of layers to project daughter fit for comparison with parent cluster
    float           m_maxIntraLayerDistance;            ///< Max value of closest intra layer approach between parent and daughter clusters
};

//------------------------------------------------------------------------------------------------------------------------------------------

inline pandora::Algorithm *BackscatteredTracksAlgorithm::Factory::CreateAlgorithm() const
{
    return new BackscatteredTracksAlgorithm();
}

#endif // #ifndef BACKSCATTERED_TRACKS_ALGORITHM_H