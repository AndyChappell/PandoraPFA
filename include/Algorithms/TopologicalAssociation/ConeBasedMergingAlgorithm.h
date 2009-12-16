/**
 *  @file   PandoraPFANew/include/Algorithms/TopologicalAssociation/ConeBasedMergingAlgorithm.h
 * 
 *  @brief  Header file for the cone based merging algorithm class.
 * 
 *  $Log: $
 */
#ifndef CONE_BASED_MERGING_ALGORITHM_H
#define CONE_BASED_MERGING_ALGORITHM_H 1

#include "Algorithms/Algorithm.h"

/**
 *  @brief  ConeBasedMergingAlgorithm class
 */
class ConeBasedMergingAlgorithm : public pandora::Algorithm
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

    typedef pandora::ClusterHelper::ClusterFitResult ClusterFitResult;
    typedef std::map<pandora::Cluster *, ClusterFitResult> ClusterFitResultMap;

    /**
     *  @brief  Prepare clusters for the cone based merging algorithm, applying pre-selection cuts and performing a mip fit
     *          to candidate parent clusters.
     * 
     *  @param  daughterVector to receive the daughter cluster vector
     *  @param  parentFitResultMap to receive the parent cluster fit result map
     */
    StatusCode PrepareClusters(pandora::ClusterVector &daughterVector, ClusterFitResultMap &parentFitResultMap) const;

    /**
     *  @brief  Get the fraction of hits in a daughter candidate cluster that are contained in a cluster defined by a mip fit
     *          to the parent candidate cluster
     * 
     *  @param  pParentCluster address of the parent candidate cluster
     *  @param  pDaughterCluster address of the daughter candidate cluster
     *  @param  parentMipFitResult the mip fit result for the parent candidate cluster
     * 
     *  @return the fraction of the daughter cluster hits contained in the cone
     */
    float GetFractionInCone(pandora::Cluster *const pParentCluster, const pandora::Cluster *const pDaughterCluster,
        const ClusterFitResult &parentMipFitResult) const;

    /**
     *  @brief  Sort clusters by ascending inner layer, and by descending number of calo hits within a layer
     * 
     *  @param  pLhs address of first cluster
     *  @param  pRhs address of second cluster
     */
    static bool SortClustersByInnerLayer(const pandora::Cluster *const pLhs, const pandora::Cluster *const pRhs);

    std::string     m_trackClusterAssociationAlgName;   ///< The name of the track-cluster association algorithm to run

    float           m_canMergeMinMipFraction;           ///< The minimum mip fraction for clusters (flagged as photons) to be merged
    float           m_canMergeMaxRms;                   ///< The maximum all hit fit rms for clusters (flagged as photons) to be merged

    unsigned int    m_minCaloHitsPerCluster;            ///< The min number of calo hits per cluster
    unsigned int    m_minLayersToShowerMax;             ///< The min number of layers between parent inner layer and shower max layer

    float           m_minConeFraction;                  ///< The min fraction of daughter hits that must lie in parent mip fit cone
    float           m_maxInnerLayerSeparation;          ///< Max distance between parent and daughter inner layer centroids
    float           m_maxInnerLayerSeparationNoTrack;   ///< Max distance between parent/daughter inner centroids when parent has no associated tracks
    float           m_coneCosineHalfAngle;              ///< Cosine of cone half angle

    float           m_minDaughterHadronicEnergy;        ///< Minimum daughter hadronic energy for merging (unless chi2 criteria are met)
    float           m_maxTrackClusterChi;               ///< Max no. standard deviations between clusters and associated track energies
    float           m_maxTrackClusterDChi2;             ///< Max diff between chi2 using parent+daughter energies and that using only parent

    float           m_minCosConeAngleWrtRadial;         ///< Min cosine of angle between cone and radial direction
    float           m_cosConeAngleWrtRadialCut1;        ///< 1st pair of cuts: Min cosine of angle between cone and radial direction
    float           m_minHitSeparationCut1;             ///< 1st pair of cuts: Max separation between cone vertex and daughter cluster hit
    float           m_cosConeAngleWrtRadialCut2;        ///< 2nd pair of cuts: Min cosine of angle between cone and radial direction
    float           m_minHitSeparationCut2;             ///< 2nd pair of cuts: Max separation between cone vertex and daughter cluster hit
};

//------------------------------------------------------------------------------------------------------------------------------------------

inline pandora::Algorithm *ConeBasedMergingAlgorithm::Factory::CreateAlgorithm() const
{
    return new ConeBasedMergingAlgorithm();
}

#endif // #ifndef CONE_BASED_MERGING_ALGORITHM_H