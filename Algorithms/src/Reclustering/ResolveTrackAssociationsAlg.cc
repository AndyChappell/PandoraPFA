/**
 *  @file   PandoraPFANew/Algorithms/src/Reclustering/ResolveTrackAssociationsAlg.cc
 * 
 *  @brief  Implementation of the resolve track associations algorithm class.
 * 
 *  $Log: $
 */

#include "Pandora/AlgorithmHeaders.h"

#include "Reclustering/ResolveTrackAssociationsAlg.h"

using namespace pandora;

StatusCode ResolveTrackAssociationsAlg::Run()
{
    // Begin by recalculating track-cluster associations
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::RunDaughterAlgorithm(*this, m_trackClusterAssociationAlgName));

    // Store copy of input cluster list in a vector
    const ClusterList *pClusterList = NULL;
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::GetCurrentClusterList(*this, pClusterList));

    ClusterVector clusterVector(pClusterList->begin(), pClusterList->end());
    std::sort(clusterVector.begin(), clusterVector.end(), Cluster::SortByInnerLayer);

    // Examine each cluster in the input list
    const unsigned int nClusters(clusterVector.size());

    for (unsigned int i = 0; i < nClusters; ++i)
    {
        Cluster *pParentCluster = clusterVector[i];

        if (NULL == pParentCluster)
            continue;

        // Check compatibility of cluster with its associated tracks
        const TrackList &trackList(pParentCluster->GetAssociatedTrackList());
        const unsigned int nTrackAssociations(trackList.size());

        if ((nTrackAssociations < m_minTrackAssociations) || (nTrackAssociations > m_maxTrackAssociations))
            continue;

        const float chi(ReclusterHelper::GetTrackClusterCompatibility(pParentCluster, trackList));

        if (chi > m_chiToAttemptReclustering)
            continue;

        if (ClusterHelper::IsClusterLeavingDetector(pParentCluster))
            continue;

        // Specify tracks and clusters to be used in reclustering
        TrackList reclusterTrackList(trackList.begin(), trackList.end());

        ClusterList reclusterClusterList;
        reclusterClusterList.insert(pParentCluster);

        UIntVector originalClusterIndices(1, i);

        // Look for potential daughter clusters to combine in the reclustering
        for (unsigned int j = 0; j < nClusters; ++j)
        {
            Cluster *pDaughterCluster = clusterVector[j];

            if ((NULL == pDaughterCluster) || (pParentCluster == pDaughterCluster) || (!pDaughterCluster->GetAssociatedTrackList().empty()))
                continue;

            if (FragmentRemovalHelper::GetFractionOfHitsInCone(pDaughterCluster, pParentCluster, m_coneCosineHalfAngle) > m_minConeFraction)
            {
                reclusterClusterList.insert(pDaughterCluster);
                originalClusterIndices.push_back(j);
            }
        }

        // Initialize reclustering with these local lists
        std::string originalClustersListName;
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::InitializeReclustering(*this, reclusterTrackList, 
            reclusterClusterList, originalClustersListName));

        // Run multiple clustering algorithms and identify the best cluster candidates produced
        std::string bestReclusterListName, bestGuessListName;
        float bestReclusterChi(chi);
        float bestReclusterChi2(chi * chi);
        float bestGuessChi(std::numeric_limits<float>::max());

        for (StringVector::const_iterator clusteringIter = m_clusteringAlgorithms.begin(), clusteringIterEnd = m_clusteringAlgorithms.end();
            clusteringIter != clusteringIterEnd; ++clusteringIter)
        {
            // Produce new cluster candidates
            std::string reclustersListName;
            const ClusterList *pReclusterList = NULL;
            PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::RunClusteringAlgorithm(*this, *clusteringIter, 
                pReclusterList, reclustersListName));

            if (pReclusterList->empty())
                continue;

            PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::RunDaughterAlgorithm(*this, m_associationAlgorithmName));
            PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::RunDaughterAlgorithm(*this, m_trackClusterAssociationAlgName));

            // Calculate figure of merit for recluster candidates. Label as best recluster candidates if applicable
            ReclusterHelper::ReclusterResult reclusterResult;

            if (STATUS_CODE_SUCCESS != ReclusterHelper::ExtractReclusterResults(pReclusterList, reclusterResult))
                continue;

            if (reclusterResult.GetMinTrackAssociationEnergy() < m_minClusterEnergyForTrackAssociation)
                continue;

            // Are recluster candidates good enough to justify replacing original clusters?
            const float reclusterChi2(reclusterResult.GetChi2PerDof());
            static const float minChi2(m_chiToAttemptReclustering * m_chiToAttemptReclustering);

            if ((bestReclusterChi2 - reclusterChi2 > m_minChi2Improvement) && (reclusterChi2 < minChi2))
            {
                bestReclusterChi = reclusterResult.GetChiPerDof();
                bestReclusterChi2 = reclusterChi2;
                bestReclusterListName = reclustersListName;
            }

            // If no ideal candidate is found, store a best guess candidate for future modification
            else if (m_shouldUseBestGuessCandidates)
            {
                if ((reclusterResult.GetNExcessTrackAssociations() > 0) && (reclusterResult.GetChi() > 0) && (reclusterResult.GetChi() < bestGuessChi))
                {
                    bestGuessChi = reclusterResult.GetChi();
                    bestGuessListName = reclustersListName;
                }
            }

            // If chi2 is very good, stop the reclustering attempts
            if(bestReclusterChi2 < m_chi2ForAutomaticClusterSelection)
                break;

            // If using ordered algorithms, chi2 is good enough and things are getting worse, stop
            if (m_usingOrderedAlgorithms && (bestReclusterChi2 < m_bestChi2ForReclusterHalt) && (reclusterChi2 > m_currentChi2ForReclusterHalt))
                break;
        }

        // If no ideal candidate constructed, can choose to use best guess candidates, which could be split by later algorithms
        if (m_shouldUseBestGuessCandidates && bestReclusterListName.empty())
        {
            bestReclusterListName = bestGuessListName;
        }

        // Tidy cluster vector, to remove addresses of deleted clusters
        if (bestReclusterListName.empty())
        {
            bestReclusterListName = originalClustersListName;
        }

        // If cannot produce satisfactory split of cluster using main clustering algorithms, use forced clustering algorithm
        if (m_shouldUseForcedClustering)
        {
            if ((bestReclusterListName == originalClustersListName) || (bestReclusterChi > m_minChiForForcedClustering))
            {
                std::string forcedListName;
                const ClusterList *pForcedClusterList = NULL;
                PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::RunClusteringAlgorithm(*this, m_forcedClusteringAlgorithmName,
                    pForcedClusterList, forcedListName));

                PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::RunDaughterAlgorithm(*this, m_trackClusterAssociationAlgName));

                ReclusterHelper::ReclusterResult forcedClusterResult;

                if (STATUS_CODE_SUCCESS == ReclusterHelper::ExtractReclusterResults(pForcedClusterList, forcedClusterResult))
                {
                    const float forcedChi2(forcedClusterResult.GetChi2PerDof());
                    const float originalChi2(chi * chi);

                    if ((originalChi2 - forcedChi2 > m_minForcedChi2Improvement) && (forcedChi2 < m_maxForcedChi2))
                        bestReclusterListName = forcedListName;
                }
            }
        }

        if (bestReclusterListName != originalClustersListName)
        {
            for (UIntVector::const_iterator iter = originalClusterIndices.begin(), iterEnd = originalClusterIndices.end(); iter != iterEnd; ++iter)
                clusterVector[*iter] = NULL;
        }

        // Choose the best recluster candidates, which may still be the originals
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::EndReclustering(*this, bestReclusterListName));
    }

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode ResolveTrackAssociationsAlg::ReadSettings(const TiXmlHandle xmlHandle)
{
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ProcessAlgorithmList(*this, xmlHandle, "clusteringAlgorithms",
        m_clusteringAlgorithms));

    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ProcessAlgorithm(*this, xmlHandle, "ClusterAssociation",
        m_associationAlgorithmName));

    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ProcessAlgorithm(*this, xmlHandle, "TrackClusterAssociation",
        m_trackClusterAssociationAlgName));

    m_minTrackAssociations = 1;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MinTrackAssociations", m_minTrackAssociations));

    if (m_minTrackAssociations < 1)
        return STATUS_CODE_INVALID_PARAMETER;

    m_maxTrackAssociations = std::numeric_limits<unsigned int>::max();
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MaxTrackAssociations", m_maxTrackAssociations));

    m_chiToAttemptReclustering = -3.f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "ChiToAttemptReclustering", m_chiToAttemptReclustering));

    m_minChi2Improvement = 1.f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MinChi2Improvement", m_minChi2Improvement));

    m_coneCosineHalfAngle = 0.9f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "ConeCosineHalfAngle", m_coneCosineHalfAngle));

    m_minConeFraction = 0.2f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MinConeFraction", m_minConeFraction));

    m_minClusterEnergyForTrackAssociation = 0.1f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MinClusterEnergyForTrackAssociation", m_minClusterEnergyForTrackAssociation));

    m_chi2ForAutomaticClusterSelection = 1.f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "Chi2ForAutomaticClusterSelection", m_chi2ForAutomaticClusterSelection));

    m_usingOrderedAlgorithms = false;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "UsingOrderedAlgorithms", m_usingOrderedAlgorithms));

    m_bestChi2ForReclusterHalt = 4.f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "BestChi2ForReclusterHalt", m_bestChi2ForReclusterHalt));

    m_currentChi2ForReclusterHalt = 16.f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "CurrentChi2ForReclusterHalt", m_currentChi2ForReclusterHalt));

    m_shouldUseBestGuessCandidates = true;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "ShouldUseBestGuessCandidates", m_shouldUseBestGuessCandidates));

    m_shouldUseForcedClustering = false;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "ShouldUseForcedClustering", m_shouldUseForcedClustering));

    if (m_shouldUseForcedClustering)
    {
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ProcessAlgorithm(*this, xmlHandle, "ForcedClustering",
            m_forcedClusteringAlgorithmName));
    }

    m_minChiForForcedClustering = 4.f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MinChiForForcedClustering", m_minChiForForcedClustering));

    m_minForcedChi2Improvement = 9.f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MinForcedChi2Improvement", m_minForcedChi2Improvement));

    m_maxForcedChi2 = 36.f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MaxForcedChi2", m_maxForcedChi2));

    return STATUS_CODE_SUCCESS;
}