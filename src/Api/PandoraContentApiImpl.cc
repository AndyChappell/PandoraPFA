/**
 *	@file	PandoraPFANew/src/Api/PandoraContentApiImpl.cc
 * 
 *	@brief	Implementation of the pandora content api class.
 * 
 *	$Log: $
 */

#include "Algorithms/Algorithm.h"

#include "Api/PandoraContentApiImpl.h"

#include "Managers/CaloHitManager.h"
#include "Managers/ClusterManager.h"
#include "Managers/MCManager.h"
#include "Managers/TrackManager.h"
#include "Managers/ParticleFlowObjectManager.h"

#include "Pandora.h"

#include <iostream>

namespace pandora
{

template <typename CLUSTER_PARAMETERS>
StatusCode PandoraContentApiImpl::CreateCluster(CLUSTER_PARAMETERS *pClusterParameters) const
{
	return m_pPandora->m_pClusterManager->CreateCluster(pClusterParameters);
}

//------------------------------------------------------------------------------------------------------------------------------------------	
	
StatusCode PandoraContentApiImpl::CreateParticleFlowObject(const PandoraContentApi::ParticleFlowObjectParameters &particleFlowObjectParameters) const
{
	return m_pPandora->m_pParticleFlowObjectManager->CreateParticleFlowObject(particleFlowObjectParameters);
}

//------------------------------------------------------------------------------------------------------------------------------------------	
	
StatusCode PandoraContentApiImpl::RunAlgorithm(const std::string &algorithmName) const
{
	Pandora::AlgorithmMap::const_iterator iter = m_pPandora->m_algorithmMap.find(algorithmName);
	
	if (m_pPandora->m_algorithmMap.end() == iter)
		return STATUS_CODE_NOT_FOUND;
	
	try
	{
		std::cout << "Running Algorithm: " << iter->first << std::endl;
		PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, iter->second->Run());
	}
	catch (StatusCodeException &statusCodeException)
	{
		std::cout << "Failure in algorithm " << iter->first << ", " << statusCodeException.ToString() << std::endl;
	}
	catch (...)
	{
		std::cout << "Failure in algorithm " << iter->first << ", unrecognized exception" << std::endl;
	}

	PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, m_pPandora->m_pCaloHitManager->ResetAfterAlgorithmCompletion(iter->second));
	PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, m_pPandora->m_pClusterManager->ResetAfterAlgorithmCompletion(iter->second));
	PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=,	m_pPandora->m_pTrackManager->ResetAfterAlgorithmCompletion(iter->second));
	
	return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------	

StatusCode PandoraContentApiImpl::MatchCaloHitsToMCPfoTargets() const
{
	UidToMCParticleMap caloHitToPfoTargetMap;
	PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pPandora->m_pMCManager->SelectPfoTargets());
	PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pPandora->m_pMCManager->CreateCaloHitToPfoTargetMap(caloHitToPfoTargetMap));
	PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pPandora->m_pCaloHitManager->MatchCaloHitsToMCPfoTargets(caloHitToPfoTargetMap));
	PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pPandora->m_pMCManager->DeleteNonPfoTargets());

	return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------	

StatusCode PandoraContentApiImpl::GetCurrentClusterList(ClusterList *const pClusterList, std::string &clusterListName) const
{
	return m_pPandora->m_pClusterManager->GetCurrentList(pClusterList, clusterListName);
}

//------------------------------------------------------------------------------------------------------------------------------------------	

StatusCode PandoraContentApiImpl::GetCurrentOrderedCaloHitList(OrderedCaloHitList *const pOrderedCaloHitList,
	std::string &orderedCaloHitListName) const
{
	return m_pPandora->m_pCaloHitManager->GetCurrentList(pOrderedCaloHitList, orderedCaloHitListName);
}

//------------------------------------------------------------------------------------------------------------------------------------------	

StatusCode PandoraContentApiImpl::GetCurrentTrackList(TrackList *const pTrackList, std::string &trackListName) const
{
	return m_pPandora->m_pTrackManager->GetCurrentList(pTrackList, trackListName);
}

//------------------------------------------------------------------------------------------------------------------------------------------	

StatusCode PandoraContentApiImpl::InitializeReclustering(const Algorithm &algorithm, const TrackList &inputTrackList,
	const ClusterList &inputClusterList, std::string &originalClustersListName) const
{
	std::string temporaryListName;
	PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pPandora->m_pTrackManager->CreateTemporaryListAndSetCurrent(&algorithm, inputTrackList, temporaryListName));
	PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pPandora->m_pCaloHitManager->CreateTemporaryListAndSetCurrent(&algorithm, inputClusterList, temporaryListName));

	std::string parentClusterListName;
	PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pPandora->m_pClusterManager->GetReclusterListName(parentClusterListName));
	PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pPandora->m_pClusterManager->MoveClustersToTemporaryListAndSetCurrent(&algorithm, parentClusterListName, originalClustersListName, &inputClusterList));

	return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------	

StatusCode PandoraContentApiImpl::EndReclustering(const Algorithm &algorithm, const std::string &selectedClusterListName) const
{
	std::string parentClusterListName;
	PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pPandora->m_pClusterManager->GetAndResetReclusterListName(parentClusterListName));
	PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pPandora->m_pClusterManager->SaveTemporaryClusters(&algorithm, parentClusterListName, selectedClusterListName));

	return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------	

StatusCode PandoraContentApiImpl::RunClusteringAlgorithm(const Algorithm &algorithm, const std::string &clusteringAlgorithmName,
	ClusterList *pNewClusterList, std::string &newClusterListName) const
{
	PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pPandora->m_pClusterManager->MakeTemporaryListAndSetCurrent(&algorithm,	newClusterListName));
	PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->RunAlgorithm(clusteringAlgorithmName));
	PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pPandora->m_pClusterManager->GetCurrentList(pNewClusterList, newClusterListName));

	return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------	

StatusCode PandoraContentApiImpl::SaveClusterListAndRemoveCaloHits(const Algorithm &algorithm, const std::string &newClusterListName,
	const std::string &currentClusterListName, const ClusterList *const pClustersToSave) const
{
	PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pPandora->m_pClusterManager->SaveTemporaryClusters(&algorithm, newClusterListName, currentClusterListName, pClustersToSave));

	const ClusterList *pNewClusterList = NULL;
	PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pPandora->m_pClusterManager->GetList(newClusterListName, pNewClusterList));
	PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pPandora->m_pCaloHitManager->RemoveCaloHitsFromCurrentList(*pNewClusterList));
	
	return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------	

StatusCode PandoraContentApiImpl::SaveClusterListAndReplaceCurrent(const Algorithm &algorithm, const std::string &newClusterListName,
	const std::string &currentClusterListName, const ClusterList *const pClustersToSave) const
{
	PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pPandora->m_pClusterManager->SaveTemporaryClusters(&algorithm, newClusterListName, currentClusterListName, pClustersToSave));
	PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pPandora->m_pClusterManager->SetCurrentList(&algorithm, newClusterListName));

	return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

PandoraContentApiImpl::PandoraContentApiImpl(Pandora *pPandora) :
	m_pPandora(pPandora)
{
}

//------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------

template StatusCode PandoraContentApiImpl::CreateCluster<CaloHit>(CaloHit *pCaloHit) const;
template StatusCode PandoraContentApiImpl::CreateCluster<InputCaloHitList>(InputCaloHitList *pInputCaloHitList) const;
template StatusCode PandoraContentApiImpl::CreateCluster<Track>(pandora::Track *pTrack) const;

} // namespace pandora
