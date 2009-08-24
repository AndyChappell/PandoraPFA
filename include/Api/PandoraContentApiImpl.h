/**
 *	@file	PandoraPFANew/include/Api/PandoraContentApiImpl.h
 *
 * 	@brief	Header file for the pandora content api implementation class.
 * 
 *	$Log: $
 */
#ifndef PANDORA_CONTENT_API_IMPL_H
#define PANDORA_CONTENT_API_IMPL_H 1

#include "StatusCodes.h"

namespace pandora
{

class Pandora;

//------------------------------------------------------------------------------------------------------------------------------------------

/**
 *	@brief PandoraContentApiImpl class
 */
class PandoraContentApiImpl
{
public:
	/**
	 *	@brief	Create a cluster
	 *
	 *	@param	pClusterParameters address of either 1) a calo hit, 2) an input calo hit list or 3) a track
	 */
	template <typename CLUSTER_PARAMETERS>
	StatusCode CreateCluster(CLUSTER_PARAMETERS *pClusterParameters) const;

	/**
	 *	@brief	Create a particle flow object
	 * 
	 *	@param	particleFlowObjectParameters the particle flow object parameters
	 */		 
	StatusCode CreateParticleFlowObject(const PandoraContentApi::ParticleFlowObjectParameters &particleFlowObjectParameters) const;
		
	/**
	 *	@brief	Run an algorithm registered with pandora
	 * 
	 *	@param	algorithmName the algorithm name
	 */
	StatusCode RunAlgorithm(const std::string &algorithmName) const;

	/**
	 *	@brief	Match calo hits to their correct mc particles for particle flow
	 */
	StatusCode MatchCaloHitsToMCPfoTargets() const;

	/**
	 *	@brief	Get the current cluster list
	 * 
	 *	@param	pClusterList to receive the address of the current cluster list
	 *	@param	clusterListName to receive the current cluster list name
	 */
	StatusCode GetCurrentClusterList(ClusterList *const pClusterList, std::string &clusterListName) const;

	/**
	 *	@brief	Get the current ordered calo hit list
	 * 
	 *	@param	pOrderedCaloHitList to receive the address of the current ordered calo hit list
	 *	@param	orderedCaloHitListName to receive the current ordered calo hit list name
	 */
	StatusCode GetCurrentOrderedCaloHitList(OrderedCaloHitList *const pOrderedCaloHitList, std::string &orderedCaloHitListName) const;

	/**
	 *	@brief	Get the current track list
	 * 
	 *	@param	pTrackList to receive the address of the current track list
	 *	@param	trackListName to receive the current track list name
	 */
	StatusCode GetCurrentTrackList(TrackList *const pTrackList, std::string &trackListName) const;

	/**
	 *	@brief	Initialize reclustering operations
	 * 
	 *	@param	algorithm the algorithm calling this function
	 *	@param	inputTrackList the input track list
	 *	@param	inputClusterList the input cluster list
	 *	@param	originalClustersListName to receive the name of the list in which the original clusters are stored
	 */
	StatusCode InitializeReclustering(const pandora::Algorithm &algorithm, const TrackList &inputTrackList, 
		const ClusterList &inputClusterList, std::string &originalClustersListName) const;

	/**
	 *	@brief	End reclustering operations
	 * 
	 *	@param	algorithm the algorithm calling this function
	 *	@param	pandora the pandora instance performing reclustering
	 *	@param	selectedClusterListName the name of the list containing the chosen recluster candidates (or the original candidates)
	 */
	StatusCode EndReclustering(const pandora::Algorithm &algorithm, const std::string &selectedClusterListName) const;

	/**
	 *	@brief	Run a clustering algorithm (an algorithm that will create new cluster objects)
	 * 
	 *	@param	algorithm the algorithm calling this function
	 *	@param	clusteringAlgorithmName the name of the clustering algorithm to run
	 *	@param	pNewClusterList the address of the new cluster list populated
	 *	@param	newClusterListName the name of the new cluster list populated
	 */
	 StatusCode RunClusteringAlgorithm(const pandora::Algorithm &algorithm, const std::string &clusteringAlgorithmName,
		ClusterList *pNewClusterList, std::string &newClusterListName) const;

	/**
	 *	@brief	Save the current cluster list and remove the constituent hits from the current ordered calo hit list
	 * 
	 *	@param	algorithm the algorithm calling this function
	 *	@param	newClusterListName the new cluster list name
	 *	@param	currentClusterListName the current cluster list name
	 *	@param	pClustersToSave a subset of the current cluster list - only clusters in both this and the current lists
	 * 			will be saved
	 */
	StatusCode SaveClusterListAndRemoveCaloHits(const pandora::Algorithm &algorithm, const std::string &newClusterListName,
		const std::string &currentClusterListName, const ClusterList *const pClustersToSave = NULL) const;

	/**
	 *	@brief	Save the current cluster list under a new name; use this new list as a permanent replacement for the current
	 * 			list (will persist outside the current algorithm)
	 * 
	 *	@param	algorithm the algorithm calling this function
	 *	@param	newClusterListName the new cluster list name
	 *	@param	currentClusterListName the current cluster list name
	 *	@param	pClustersToSave a subset of the current cluster list - only clusters in both this and the current lists
	 * 			will be saved
	 */
	StatusCode SaveClusterListAndReplaceCurrent(const pandora::Algorithm &algorithm, const std::string &newClusterListName,
		const std::string &currentClusterListName, const ClusterList *const pClustersToSave = NULL) const;

private:
	/**
	 *	@brief	Constructor
	 * 
	 *	@param	pPandora address of the pandora object to interface
	 */
	PandoraContentApiImpl(Pandora *pPandora);

	Pandora	*m_pPandora;	///< The pandora object to provide an interface to
	
	friend class Pandora;
};

} // namespace pandora

#endif // #ifndef PANDORA_CONTENT_API_IMPL_H
