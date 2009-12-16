/**
 *  @file   PandoraPFANew/src/Algorithms/TopologicalAssociation/ConeBasedMergingAlgorithm.cc
 * 
 *  @brief  Implementation of the cone based merging algorithm class.
 * 
 *  $Log: $
 */

#include "Algorithms/TopologicalAssociation/ConeBasedMergingAlgorithm.h"

#include <algorithm>
#include <limits>

using namespace pandora;

StatusCode ConeBasedMergingAlgorithm::Run()
{
    // Begin by recalculating track-cluster associations
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::RunDaughterAlgorithm(*this, m_trackClusterAssociationAlgName));

    // Then prepare clusters for this merging algorithm
    ClusterVector daughterVector;
    ClusterFitResultMap parentFitResultMap;
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->PrepareClusters(daughterVector, parentFitResultMap));

    // Loop over daughter candidates and, for each, examine all possible parents
    for (ClusterVector::reverse_iterator iterI = daughterVector.rbegin(), iterIEnd = daughterVector.rend(); iterI != iterIEnd; ++iterI)
    {
        Cluster *pDaughterCluster = *iterI;

        if (NULL == pDaughterCluster)
            continue;

        Cluster *pBestParentCluster(NULL);
        float highestConeFraction(m_minConeFraction);
        const PseudoLayer daughterInnerLayer(pDaughterCluster->GetInnerPseudoLayer());

        for (ClusterFitResultMap::const_iterator iterJ = parentFitResultMap.begin(); iterJ != parentFitResultMap.end(); ++iterJ)
        {
            Cluster *pParentCluster = iterJ->first;

            if (pDaughterCluster == pParentCluster)
                continue;

            // Cut on inner layer separation
            const CartesianVector parentInnerLayerCentroid(pParentCluster->GetCentroid(pParentCluster->GetInnerPseudoLayer()));
            const CartesianVector daughterInnerLayerCentroid(pDaughterCluster->GetCentroid(daughterInnerLayer));

            const float innerLayerSeparation((parentInnerLayerCentroid - daughterInnerLayerCentroid).GetMagnitude());

            if (innerLayerSeparation > m_maxInnerLayerSeparation)
                continue;

            if (pParentCluster->GetAssociatedTrackList().empty() && (innerLayerSeparation > m_maxInnerLayerSeparationNoTrack))
                continue;

            // The best parent cluster is that for which a cone (around its mip fit) encloses the most daughter cluster hits
            const ClusterFitResult &mipFitResult = iterJ->second;
            const float fractionInCone(this->GetFractionInCone(pParentCluster, pDaughterCluster, mipFitResult));

            if (fractionInCone > highestConeFraction)
            {
                highestConeFraction = fractionInCone;
                pBestParentCluster = pParentCluster;
            }
        }

        if (NULL != pBestParentCluster)
        {
            // Check consistency of cluster energy and energy of associated tracks
            const TrackList &trackList(pBestParentCluster->GetAssociatedTrackList());

            if (!trackList.empty())
            {
                float trackEnergySum(0.);

                for (TrackList::const_iterator trackIter = trackList.begin(), trackIterEnd = trackList.end(); trackIter != trackIterEnd; ++trackIter)
                    trackEnergySum += (*trackIter)->GetEnergyAtDca();

                if (trackEnergySum > 0.)
                {
                    static const float hadronicEnergyResolution(PandoraSettings::GetInstance()->GetHadronicEnergyResolution());
                    const float sigmaE(hadronicEnergyResolution * trackEnergySum / std::sqrt(trackEnergySum));

                    if (0. == sigmaE)
                        continue;

                    const float clusterEnergySum = (pBestParentCluster->GetHadronicEnergy() + pDaughterCluster->GetHadronicEnergy());

                    const float chi((clusterEnergySum - trackEnergySum) / sigmaE);
                    const float chi0((pBestParentCluster->GetHadronicEnergy() - trackEnergySum) / sigmaE);

                    bool canMergeClusters(pDaughterCluster->GetHadronicEnergy() > m_minDaughterHadronicEnergy);

                    if (!canMergeClusters)
                    {
                        if ((chi > m_maxTrackClusterChi) || ((chi * chi - chi0 * chi0) > m_maxTrackClusterDChi2))
                            continue;
                    }
                }
            }

            // Finally, merge the clusters
            (*iterI) = NULL;
            parentFitResultMap.erase(pDaughterCluster);
            PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::MergeAndDeleteClusters(*this, pBestParentCluster, pDaughterCluster));
        }
    }

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode ConeBasedMergingAlgorithm::PrepareClusters(ClusterVector &daughterVector, ClusterFitResultMap &parentFitResultMap) const
{
    const ClusterList *pClusterList = NULL;
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::GetCurrentClusterList(*this, pClusterList));

    // Store cluster list in a vector and sort by descending inner layer, and by number of hits within a layer
    for (ClusterList::const_iterator iter = pClusterList->begin(), iterEnd = pClusterList->end(); iter != iterEnd; ++iter)
    {
        if ((*iter)->GetNCaloHits() < m_minCaloHitsPerCluster)
            continue;

        if (ClusterHelper::CanMergeCluster(*iter, m_canMergeMinMipFraction, m_canMergeMaxRms))
            daughterVector.push_back(*iter);
    }

    std::sort(daughterVector.begin(), daughterVector.end(), ConeBasedMergingAlgorithm::SortClustersByInnerLayer);

    // Perform a mip fit to all parent candidate clusters
    for (ClusterList::const_iterator iter = pClusterList->begin(), iterEnd = pClusterList->end(); iter != iterEnd; ++iter)
    {
        Cluster *pCluster = *iter;

        if (pCluster->GetNCaloHits() < m_minCaloHitsPerCluster)
            continue;

        if (!ClusterHelper::CanMergeCluster(pCluster, m_canMergeMinMipFraction, m_canMergeMaxRms))
            continue;

        const PseudoLayer innerLayer(pCluster->GetInnerPseudoLayer());
        const PseudoLayer showerMaxLayer(pCluster->GetShowerMaxLayer());

        if ((innerLayer > showerMaxLayer) || ((showerMaxLayer - innerLayer) < m_minLayersToShowerMax))
            continue;

        const PseudoLayer fitEndLayer((showerMaxLayer > 1) ? showerMaxLayer - 1 : 0);

        ClusterFitResult mipFitResult;
        if (STATUS_CODE_SUCCESS != ClusterHelper::FitLayers(pCluster, innerLayer, fitEndLayer, mipFitResult))
            continue;

        if (!parentFitResultMap.insert(ClusterFitResultMap::value_type(pCluster, mipFitResult)).second)
            return STATUS_CODE_FAILURE;
    }

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

float ConeBasedMergingAlgorithm::GetFractionInCone(Cluster *const pParentCluster, const Cluster *const pDaughterCluster,
    const ClusterFitResult &parentMipFitResult) const
{
    // Apply preliminary checks
    const unsigned int nDaughterCaloHits(pDaughterCluster->GetNCaloHits());

    if (!parentMipFitResult.IsFitSuccessful() || (0 == nDaughterCaloHits))
        return 0.;

    const PseudoLayer parentShowerMaxLayer(pParentCluster->GetShowerMaxLayer());

    if (pDaughterCluster->GetInnerPseudoLayer() < parentShowerMaxLayer)
        return 0.;

    // Identify cone vertex
    const CartesianVector &parentMipFitDirection(parentMipFitResult.GetDirection());
    const CartesianVector &parentMipFitIntercept(parentMipFitResult.GetIntercept());

    const CartesianVector showerMaxDifference(pParentCluster->GetCentroid(parentShowerMaxLayer) - parentMipFitIntercept);
    const float parallelDistanceToShowerMax(showerMaxDifference.GetDotProduct(parentMipFitDirection));
    const CartesianVector coneVertex(parentMipFitIntercept + (parentMipFitDirection * parallelDistanceToShowerMax));

    // Don't allow large distance associations at low angle
    const float cosConeAngleWrtRadial(coneVertex.GetUnitVector().GetDotProduct(parentMipFitDirection));

    if (cosConeAngleWrtRadial < m_minCosConeAngleWrtRadial)
        return 0.;

    // Count daughter cluster hits in cone
    unsigned int nHitsInCone(0);
    float minHitSeparation(std::numeric_limits<float>::max());
    const OrderedCaloHitList &orderedCaloHitList(pDaughterCluster->GetOrderedCaloHitList());

    for (OrderedCaloHitList::const_iterator iter = orderedCaloHitList.begin(), iterEnd = orderedCaloHitList.end(); iter != iterEnd; ++iter)
    {
        for (CaloHitList::const_iterator hitIter = iter->second->begin(), hitIterEnd = iter->second->end(); hitIter != hitIterEnd; ++hitIter)
        {
            const CartesianVector positionDifference((*hitIter)->GetPositionVector() - coneVertex);
            const float hitSeparation(positionDifference.GetMagnitude());

            if (0. == hitSeparation)
                throw StatusCodeException(STATUS_CODE_FAILURE);

            if (hitSeparation < minHitSeparation)
                minHitSeparation = hitSeparation;

            const float cosTheta(parentMipFitDirection.GetDotProduct(positionDifference) / hitSeparation);

            if (cosTheta > m_coneCosineHalfAngle)
                nHitsInCone++;
        }
    }

    // Further checks to prevent large distance associations at low angle
    if ( ((cosConeAngleWrtRadial < m_cosConeAngleWrtRadialCut1) && (minHitSeparation > m_minHitSeparationCut1)) ||
         ((cosConeAngleWrtRadial < m_cosConeAngleWrtRadialCut2) && (minHitSeparation > m_minHitSeparationCut2)) )
    {
        return 0.;
    }

    return static_cast<float>(nHitsInCone) / static_cast<float>(nDaughterCaloHits);
}

//------------------------------------------------------------------------------------------------------------------------------------------

bool ConeBasedMergingAlgorithm::SortClustersByInnerLayer(const Cluster *const pLhs, const Cluster *const pRhs)
{
    const PseudoLayer innerLayerLhs(pLhs->GetInnerPseudoLayer()), innerLayerRhs(pRhs->GetInnerPseudoLayer());

    if (innerLayerLhs != innerLayerRhs)
        return (innerLayerLhs < innerLayerRhs);

    return (pLhs->GetNCaloHits() > pRhs->GetNCaloHits());
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode ConeBasedMergingAlgorithm::ReadSettings(const TiXmlHandle xmlHandle)
{
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ProcessFirstAlgorithm(*this, xmlHandle, m_trackClusterAssociationAlgName));

    m_canMergeMinMipFraction = 0.7f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "CanMergeMinMipFraction", m_canMergeMinMipFraction));

    m_canMergeMaxRms = 5.f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "CanMergeMaxRms", m_canMergeMaxRms));

    m_minCaloHitsPerCluster = 6;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MinCaloHitsPerCluster", m_minCaloHitsPerCluster));

    m_minLayersToShowerMax = 4;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MinLayersToShowerMax", m_minLayersToShowerMax));

    m_minConeFraction = 0.5f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MinConeFraction", m_minConeFraction));

    m_maxInnerLayerSeparation = 1000.f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MaxInnerLayerSeparation", m_maxInnerLayerSeparation));

    m_maxInnerLayerSeparationNoTrack = 250.f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MaxInnerLayerSeparationNoTrack", m_maxInnerLayerSeparationNoTrack));

    m_coneCosineHalfAngle = 0.9f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "ConeCosineHalfAngle", m_coneCosineHalfAngle));

    m_minDaughterHadronicEnergy = 1.f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MinDaughterHadronicEnergy", m_minDaughterHadronicEnergy));

    m_maxTrackClusterChi = 2.5f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MaxTrackClusterChi", m_maxTrackClusterChi));

    m_maxTrackClusterDChi2 = 1.f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MaxTrackClusterDChi2", m_maxTrackClusterDChi2));

    m_minCosConeAngleWrtRadial = 0.25f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MinCosConeAngleWrtRadial", m_minCosConeAngleWrtRadial));

    m_cosConeAngleWrtRadialCut1 = 0.5f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "CosConeAngleWrtRadialCut1", m_cosConeAngleWrtRadialCut1));

    m_minHitSeparationCut1 = 1000.f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MinHitSeparationCut1", m_minHitSeparationCut1));

    m_cosConeAngleWrtRadialCut2 = 0.75f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "CosConeAngleWrtRadialCut2", m_cosConeAngleWrtRadialCut2));

    m_minHitSeparationCut2 = 1500.f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MinHitSeparationCut2", m_minHitSeparationCut2));

    return STATUS_CODE_SUCCESS;
}