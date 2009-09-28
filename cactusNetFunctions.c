#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "commonC.h"
#include "fastCMaths.h"
#include "bioioC.h"
#include "hashTableC.h"
#include "pinchGraph.h"
#include "cactusGraph.h"
#include "net.h"

////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////
//Functions to construct pinch graphs from nets.
////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////

struct PinchEdge *hookUpEdge(struct Segment *segment, struct PinchGraph *pinchGraph,
		struct PinchVertex *vertex2, struct PinchVertex *vertex3) {
	struct PinchEdge *edge;

	edge = constructPinchEdge(segment);

	//Connect up each end of the black edge.
	vertex2 = constructPinchVertex(pinchGraph, -1);
	edge->from = vertex2;
	edge->rEdge->to = vertex2;
	insertBlackEdge(vertex2, edge);

	vertex3 = constructPinchVertex(pinchGraph, -1);
	edge->to = vertex3;
	edge->rEdge->from = vertex3;
	insertBlackEdge(vertex3, edge->rEdge);

	//Now add segments connected to edges to the graph.
	avl_insert(pinchGraph->edges, edge);
	avl_insert(pinchGraph->edges, edge->rEdge);

	return edge;
}

struct PinchGraph *constructPinchGraph(Net *net,
		struct List *contigIndexToContigStrings,
		struct IntList *contigIndexToContigStart) {
	struct PinchGraph *graph;
	Net_EndIterator *endIterator;
	End_InstanceIterator *instanceIterator;
	End *end;
	EndInstance *endInstance;
	EndInstance *endInstance2;
	struct PinchVertex *sourceVertex;
	struct PinchVertex *pinchVertex;
	struct PinchVertex *pinchVertex2;
	struct PinchEdge *leftCapEdge;
	struct PinchEdge *edge;
	struct PinchEdge *rightCapEdge;
	struct hashtable *hash;
	struct hashtable *hash2;
	int32_t i;
	int32_t length;

	//make basic object.
	graph = pinchGraph_construct();
	sourceVertex = graph->vertices->list[0];

	//make hashes for ends to vertices
	hash = create_hashtable(net_getEndNumber(net) * 2,
				 hashtable_stringHashKey, hashtable_stringEqualKey,
				 NULL, NULL);
	hash2 = create_hashtable(net_getEndNumber(net) * 2,
					 hashtable_stringHashKey, hashtable_stringEqualKey,
					 NULL, NULL);

	//for each cap, build a pair of vertices
	endIterator = net_getEndIterator(net);
	while((end = net_getNextEnd(endIterator)) != NULL) {
		pinchVertex = constructPinchVertex(graph, -1);
		pinchVertex2 = constructPinchVertex(graph, -1);
		//connect to source.
		if(end_isCap(end)) {
			connectVertices(sourceVertex, pinchVertex);
		}
		hashtable_insert(hash, (char *)end_getName(end), pinchVertex);
		hashtable_insert(hash2, (char *)end_getName(end), pinchVertex2);
	}
	net_destructEndIterator(endIterator);

	endIterator = net_getEndIterator(net);
	while((end = net_getNextEnd(endIterator)) != NULL) {
		instanceIterator = end_getInstanceIterator(end);
		while((endInstance = end_getNext(instanceIterator)) != NULL) {
			if(endInstance_getStrand(endInstance)) {
				endInstance2 = endInstance_getAdjacency(endInstance);
				assert(endInstance_getStrand(endInstance2));

				//Make black edges for caps/stubs on left end
				listAppend(contigIndexToContigStrings, endInstance_getCompleteName(endInstance));
				i = endInstance_getCoordinate(endInstance)+1;
				intListAppend(contigIndexToContigStart, i);
				leftCapEdge = hookUpEdge(constructSegment(contigIndexToContigStrings->length-1, i, i), graph,
						hashtable_search(hash, (void *)end_getName(endInstance_getEnd(endInstance))),
						hashtable_search(hash2, (void *)end_getName(endInstance_getEnd(endInstance))));

				//Construct the middle sequence.
				//This order is important. It ensures that the contig index of the stub/cap instance
				//to the left of the sequence is always one less than
				//contig index of the actual sequence.
				//Similarly it ensures that the contig index of the stub/cap instance
				//to the right of the sequence is always one greater than the contig index of the actual sequence!
				//This allows us to unambiguously identify which stub/cap instances lie to the left and right of which sequences,
				//useful if we unalign edges and need to work out there original (grey) adjacencies with a stub.
				listAppend(contigIndexToContigStrings, stringCopy(sequence_getName(endInstance_getSequence(endInstance))));
				intListAppend(contigIndexToContigStart, i+2);
				length = endInstance_getCoordinate(endInstance2) - endInstance_getCoordinate(endInstance) - 1;
				assert(length >= 0);
				if(length > 0) {
					edge = hookUpEdge(constructSegment(contigIndexToContigStrings->length-1, i+1, i+length), graph,
							constructPinchVertex(graph, -1), constructPinchVertex(graph, -1));
				}

				//Construct the right cap/stub
				listAppend(contigIndexToContigStrings, endInstance_getCompleteName(endInstance));
				i = endInstance_getCoordinate(endInstance2)+1;
				intListAppend(contigIndexToContigStart, i);
				rightCapEdge = hookUpEdge(constructSegment(contigIndexToContigStrings->length-1, i, i), graph,
						hashtable_search(hash, (void *)end_getName(endInstance_getEnd(endInstance2))),
						hashtable_search(hash2, (void *)end_getName(endInstance_getEnd(endInstance2))));

				//Connect the edges
				if(length > 0) {
					assert(edge != NULL);
					connectVertices(leftCapEdge->to, edge->from);
					connectVertices(edge->to, rightCapEdge->from);
				}
				else {
					connectVertices(leftCapEdge->to, rightCapEdge->from);
				}
			}
		}
		end_destructInstanceIterator(instanceIterator);
	}
	net_destructEndIterator(endIterator);

	//Cleanup the hashes
	hashtable_destroy(hash, FALSE, FALSE);
	hashtable_destroy(hash2, FALSE, FALSE);

	return graph;
}

////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////
//Functions to construct nets.
////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////

struct CactusEdge *getNonDeadEndOfStubOrCapCactusEdge(struct CactusEdge *edge, struct PinchGraph *pinchGraph) {
	struct PinchEdge *pinchEdge;
	pinchEdge = cactusEdgeToFirstPinchEdge(edge, pinchGraph);
#ifdef BEN_DEBUG
	assert(isAStubOrCapCactusEdge(edge, pinchGraph));
	assert(isADeadEnd(pinchEdge->from) || isADeadEnd(pinchEdge->to));
#endif
	return isADeadEnd(pinchEdge->from) ? edge->rEdge : edge;
}

char *cactusEdgeToAtomName(struct CactusEdge *edge, struct PinchGraph *pinchGraph, struct hashtable *names) {
	char *cA = (char *)hashtable_search(names, cactusEdgeToFirstPinchEdge(edge, pinchGraph));
#ifdef BEN_DEBUG
	assert(cA != NULL);
#endif
	return removeInstance(cA);
}

const char *cactusEdgeToEndName(struct CactusEdge *edge, struct PinchGraph *pinchGraph, struct hashtable *names) {
	char *cA = (char *)hashtable_search(names, cactusEdgeToFirstPinchEdge(edge, pinchGraph)->from);
#ifdef BEN_DEBUG
	assert(cA != NULL);
#endif
	return cA;
}

Sequence *copySequence(Net *parentNet, Net *net, const char *name) {
	Sequence *sequence = net_getSequence(parentNet, name);
	if(net_getSequence(net, sequence_getName(sequence)) == NULL) {
		sequence_copyConstruct(sequence, net);
	}
	return sequence;
}

End *constructEndFromCactusEdge(struct CactusEdge *edge,
		struct PinchGraph *pinchGraph, struct hashtable *names,
		Net *net, struct List *contigIndexToContigStrings, Net *parentNet) {
	int32_t i;
	End *end;
	struct Segment *segment;
	char *instanceName;
	end = end_construct(cactusEdgeToEndName(edge, pinchGraph, names), net);
	for(i=0; i<edge->segments->length; i++) {
		segment = edge->segments->list[i];
		instanceName = getInstance(hashtable_search(names,
										getContainingBlackEdge(pinchGraph, segment->contig, segment->start)));
		endInstance_constructWithCoordinates(instanceName, end,
				(segment->start > 0 ? segment->start : -segment->start) - 1, segment->start > 0,
				copySequence(parentNet, net, contigIndexToContigStrings->list[segment->contig]));
		free(instanceName);
	}
	return end;
}

Atom *constructAtomFromCactusEdge(struct CactusEdge *edge,
		struct PinchGraph *pinchGraph, struct hashtable *names,
		Net *net, struct List *contigIndexToContigStrings,
		Net *parentNet) {
	/*
	 * Constructs an atom and two connected ends.
	 */
	int32_t i;
	Atom *atom;
	struct Segment *segment;
	char *name;
	name = cactusEdgeToAtomName(edge, pinchGraph, names);
	segment = edge->segments->list[0];
	atom = atom_construct(name, segment->end - segment->start + 1, net);
	free(name);
	for(i=0; i<edge->segments->length; i++) {
		segment = edge->segments->list[i];
		atomInstance_constructWithCoordinates(getInstance(hashtable_search(names,
							   getContainingBlackEdge(pinchGraph, segment->contig, segment->start))),
							   atom, (segment->start > 0 ? segment->start : -segment->start) - 1, segment->start > 0,
							   copySequence(parentNet, net, contigIndexToContigStrings->list[segment->contig]));
	}
	return atom;
}

struct List *addEnvelopedStubEnds(Net *net) {
	/*
	 * For each net contained within a link in a chain, adds the encompassing ends
	 * of the chain to the nested net.
	 */
	int32_t j;
	End *end;
	Net *net2;
	struct List *list;
	End_InstanceIterator *endIterator;
	AdjacencyComponent_EndIterator *adjacencyIterator;
	AdjacencyComponent *adjacencyComponent;

	adjacencyIterator = net_getAdjacencyComponentIterator(net);
	while((adjacencyComponent = net_getNextAdjacencyComponent(adjacencyIterator)) != NULL) {
		net2 = adjacencyComponent_getNestedNet(adjacencyComponent);
		list = addEnvelopedStubEnds(net2);
		for(j=0; j<list->length; j++) {
			end_copyConstruct(list->list[j], net);
		}
		destructList(list);
	}
	net_destructAdjacencyComponentIterator(adjacencyIterator);

	list = constructEmptyList(0, NULL);
	endIterator = net_getEndIterator(net);
	while((end = net_getNextEnd(endIterator)) != NULL) {
		if(end_isStub(end)) {
			listAppend(list, end);
		}
	}
	net_destructEndIterator(endIterator);
	return list;
}

void addEnvelopingEnds(Net *net) {
	/*
	 * For each net contained within a link in a chain, adds the encompassing ends of the chain to the
	 * nested net.
	 */
	int32_t i;
	Link *link;
	AdjacencyComponent *adjacencyComponent;
	Net *net2;

	for(i=0; i<net_getChainNumber(net); i++) {
		link = chain_getLink(net_getChain(net, i), 0);
		while(link != NULL) {
			adjacencyComponent = link_getAdjacencyComponent(link);
			net2 = adjacencyComponent_getNestedNet(adjacencyComponent);
			end_copyConstruct(link_getLeft(link), net2);
			end_copyConstruct(link_getRight(link), net2);
			//Do recursive call.
			addEnvelopingEnds(net2);
			link = link_getNextLink(link);
		}
	}
}

struct PinchEdge *getOtherEnd(struct PinchGraph *graph, Net *net, struct hashtable *names,
		struct PinchEdge *edge) {
	/*
	 * Gets the other end on a sequence starting from the sequence given by the edge.
	 */
	struct PinchEdge *edge2;
#ifdef BEN_DEBUG
	assert((edge->segment->contig % 3) != 2);
	assert(edge->segment->start >= 1);
#endif

	while(TRUE) {
		edge2 = getNextEdge(graph, edge);
		if(net_getEnd(net, hashtable_search(names, edge2)) != NULL) {
			return edge2;
		}
		edge = edge2;
	}
	assert(FALSE);
}

void addAdjacenciesToEndsP(Net *net,
						  struct PinchGraph *pinchGraph,
						  struct hashtable *endsToVertices,
						  struct hashtable *names) {
	/*
	 * Links the ends in a net together by their adjacencies.
	 */
	End *end;
	EndInstance *endInstance;
	EndInstance *endInstance2;
	struct PinchVertex *vertex;
	struct PinchEdge *edge;
	struct PinchEdge *edge2;
	AdjacencyComponent *adjacencyComponent;
	void *blackEdgeIterator;
	Net_EndIterator *endIterator;
	Net_AdjacencyComponentIterator *adjacencyIterator;
	const char *name;

	endIterator = net_getEndIterator(net);
	while((end = net_getNextEnd(endIterator)) != NULL) {
		vertex = hashtable_search(endsToVertices, (char *)end_getName(end));
		blackEdgeIterator = getBlackEdgeIterator(vertex);
		edge = getNextBlackEdge(vertex, blackEdgeIterator);
		while(edge != NULL) {
			endInstance = end_getInstance(end, netMisc_getInstanceNameStatic(hashtable_search(names, edge->rEdge)));
			edge2 = getOtherEnd(pinchGraph, net, names, edge->rEdge);
			name = hashtable_search(names, edge2);
			endInstance2 = end_getInstance(net_getEnd(net, netMisc_getElementNameStatic(name)), name);
			//link them
			endInstance_makeAdjacent1(endInstance, endInstance2); //this is done reciprocally.
			edge = getNextBlackEdge(vertex, blackEdgeIterator);
		}
		destructBlackEdgeIterator(blackEdgeIterator);
	}
	net_destructEndIterator(endIterator);

	//Do for each level of the net.
	adjacencyIterator = net_getAdjacencyComponentIterator(net);
	while((adjacencyComponent = net_getNextAdjacencyComponent(adjacencyIterator)) != NULL) {
		addAdjacenciesToEndsP(adjacencyComponent_getNestedNet(adjacencyComponent), pinchGraph, endsToVertices, names);
	}
	net_destructAdjacencyComponentIterator(adjacencyIterator);
}

void addAdjacenciesToEnds(Net *net,
						  struct PinchGraph *pinchGraph,
						  struct hashtable *names) {
	/*
	 * Creates the adjacencies between the ends for in every net in a hierarchy of nets.
	 */
	int32_t i;
	struct hashtable *endsToVertices;
	char *name;
	struct PinchVertex *vertex;
	//Constructs hash table, then calls recursive function.
	endsToVertices = create_hashtable(pinchGraph->vertices->length*2,
			 hashtable_stringHashKey, hashtable_stringEqualKey,
			 NULL, NULL);

	for(i=0; i<pinchGraph->vertices->length; i++) {
		vertex = pinchGraph->vertices->list[i];
		name = hashtable_search(names, vertex);
#ifdef BEN_DEBUG
		assert(name != NULL);
#endif
		hashtable_insert(endsToVertices, name, vertex);
	}
	addAdjacenciesToEndsP(net, pinchGraph, endsToVertices, names);
	hashtable_destroy(endsToVertices, FALSE, FALSE);
}

void addAdjacencyComponentsP(Net *net, Net *nestedNet, End *end) {
	End_InstanceIterator *endInstanceIterator;
	EndInstance *endInstance;
	End *end2;
	end_copyConstruct(end, nestedNet);
	endInstanceIterator = end_getInstanceIterator(end);
	while((endInstance = end_getNext(endInstanceIterator)) != NULL) {
		end2 = endInstance_getEnd(endInstance);
		if(end_getAdjacencyComponent(end2) == NULL) {
			addAdjacencyComponentsP(net, nestedNet, end);
		}
#ifdef BEN_DEBUG
		else {
			assert(end_getAdjacencyComponent(end) == end_getAdjacencyComponent(end));
		}
#endif
	}
	end_destructInstanceIterator(endInstanceIterator);
}

void addAdjacencyComponents(Net *net, const char *(*getUniqueName)()) {
	/*
	 * Adds adjacency components to each net.
	 */
	Net_EndIterator *endIterator;
	Net_AdjacencyComponentIterator *adjacencyIterator;
	End *end;
	AdjacencyComponent *adjacencyComponent;
	Net *nestedNet;

	endIterator = net_getEndIterator(net);
	while((end = net_getNextEnd(endIterator)) != NULL) {
		adjacencyComponent = end_getAdjacencyComponent(end);
		if(adjacencyComponent == NULL) {
			nestedNet = net_construct(getUniqueName(), net_getNetDisk(net));
			adjacencyComponent_construct(net, nestedNet);
			addAdjacencyComponentsP(net, nestedNet, end);
		}
		adjacencyComponent_updateContainedEnds(adjacencyComponent); //ensure adjacency component is properly initialised.
	}
	net_destructEndIterator(endIterator);

	//Do for each level of the net.
	adjacencyIterator = net_getAdjacencyComponentIterator(net);
	while((adjacencyComponent = net_getNextAdjacencyComponent(adjacencyIterator)) != NULL) {
		addAdjacencyComponents(adjacencyComponent_getNestedNet(adjacencyComponent), getUniqueName);
	}
	net_destructAdjacencyComponentIterator(adjacencyIterator);
}

void addSequencesToNet(Net *net) {
	/*
	 * Adds sequences to each level of the net.
	 */
	Net_AdjacencyComponentIterator *adjacencyIterator;
	AdjacencyComponent *adjacencyComponent;
	Net_AtomIterator *atomIterator;
	Atom *atom;
	Atom_InstanceIterator *atomInstanceIterator;
	AtomInstance *atomInstance;

	//for each atom in net, check that net contains the sequences.
	atomIterator = net_getAtomIterator(net);
	while((atom = net_getNextAtom(atomIterator)) != NULL) {
		atomInstanceIterator = atom_getInstanceIterator(atom);
		while((atomInstance = atom_getNext(atomInstanceIterator)) != NULL) {
			net_addSequence(net, atomInstance_getSequence(atomInstance));
		}
	}

	//Do for each level of the net.
	adjacencyIterator = net_getAdjacencyComponentIterator(net);
	while((adjacencyComponent = net_getNextAdjacencyComponent(adjacencyIterator)) != NULL) {
		addSequencesToNet(adjacencyComponent_getNestedNet(adjacencyComponent));
	}
	net_destructAdjacencyComponentIterator(adjacencyIterator);
}

Net *constructNetFromInputs(
		Net *parentNet,
		struct CactusGraph *cactusGraph,
		struct PinchGraph *pinchGraph, struct hashtable *names, struct List *chosenAtoms,
		struct List *contigIndexToContigStrings, const char *(*getUniqueName)()) {
	Net *net;
	Chain *chain;
	AdjacencyComponent *adjacencyComponent;
	struct CactusVertex *cactusVertex;
	struct CactusEdge *cactusEdge;
	struct List *biConnectedComponent;
	struct List *list;
	struct List *biConnectedComponents;
	void **nets;
	void **parentNets;
	int32_t *vertexDiscoveryTimes;
	int32_t *mergedVertexIDs;
	int32_t i, j, k;
	struct hashtable *chosenAtomsHash;

	logDebug("Building the net\n");

	////////////////////////////////////////////////
	//Get sorted bi-connected components.
	////////////////////////////////////////////////

	biConnectedComponents = computeSortedBiConnectedComponents(cactusGraph);

	////////////////////////////////////////////////
	//Get DFS numbering on cactus vertices
	////////////////////////////////////////////////

	vertexDiscoveryTimes = getDFSDiscoveryTimes(cactusGraph);

	////////////////////////////////////////////////
	//Prune the cactus graph to include only those edges relevant to the desired net.
	////////////////////////////////////////////////

	chosenAtomsHash = create_hashtable(chosenAtoms->length*2,
							 hashtable_key, hashtable_equalKey,
							 NULL, NULL);
	for(i=0; i<chosenAtoms->length; i++) {
		hashtable_insert(chosenAtomsHash, chosenAtoms->list[i], &i);
	}

	mergedVertexIDs = malloc(sizeof(int32_t)*cactusGraph->vertices->length);
	for(i=0; i<cactusGraph->vertices->length; i++) {
		mergedVertexIDs[i] = ((struct CactusVertex *)cactusGraph->vertices->list[i])->vertexID;
	}
	for(i=0; i<biConnectedComponents->length; i++) {
		biConnectedComponent = biConnectedComponents->list[i];
		list = constructEmptyList(0, NULL);
		for(j=0; j<biConnectedComponent->length; j++) {
			cactusEdge = biConnectedComponent->list[j];
			if(isAStubOrCapCactusEdge(cactusEdge, pinchGraph) || hashtable_search(chosenAtomsHash, cactusEdge) != NULL) {
				//merge vertices
				if(vertexDiscoveryTimes[cactusEdge->from->vertexID] < vertexDiscoveryTimes[cactusEdge->to->vertexID]) {
					mergedVertexIDs[cactusEdge->to->vertexID] = mergedVertexIDs[cactusEdge->from->vertexID];
				}
				else if (vertexDiscoveryTimes[cactusEdge->from->vertexID] > vertexDiscoveryTimes[cactusEdge->to->vertexID]) {
					for(k=0; k <= j; k++) {
						cactusVertex = ((struct CactusEdge *)biConnectedComponent->list[j])->from;
						if(mergedVertexIDs[cactusVertex->vertexID] == mergedVertexIDs[cactusEdge->from->vertexID]) {
							mergedVertexIDs[cactusVertex->vertexID] = mergedVertexIDs[cactusEdge->to->vertexID];
						}
					}
				}
			}
			else {
				listAppend(list, cactusEdge);
			}
		}
		destructList(biConnectedComponent);
		biConnectedComponents->list[i] = list;
	}

	////////////////////////////////////////////////
	//Construct chain for each cycle.
	////////////////////////////////////////////////

	nets = mallocLocal(sizeof(void *) * cactusGraph->vertices->length);
	for(i=1; i<cactusGraph->vertices->length; i++) {
		nets[i] = NULL;
	}
	parentNets = mallocLocal(sizeof(void *) * biConnectedComponents->length);
	for(i=0; i<biConnectedComponents->length; i++) {
		biConnectedComponent = biConnectedComponents->list[i];
		cactusEdge = biConnectedComponent->list[0];
		//get the net.
		net = nets[mergedVertexIDs[cactusEdge->from->vertexID]];
		if(net == NULL) {
			net = net_construct(getUniqueName(), net_getNetDisk(parentNet));
			nets[mergedVertexIDs[cactusEdge->from->vertexID]] = net;
		}
		parentNets[i] = net;

		//Make the atoms and ends
		for(j=0; j<biConnectedComponent->length; j++) {
			cactusEdge = biConnectedComponent->list[j];
			if(!isAStubOrCapCactusEdge(cactusEdge, pinchGraph)) {
				constructAtomFromCactusEdge(cactusEdge, pinchGraph, names, net, contigIndexToContigStrings, parentNet);
			}
			else {
				constructEndFromCactusEdge(getNonDeadEndOfStubOrCapCactusEdge(cactusEdge, pinchGraph),
						pinchGraph, names, net, contigIndexToContigStrings, parentNet);
			}
		}
	}
	logDebug("Constructed atoms and nets for the cycle.\n");

	////////////////////////////////////////////////
	//Link nets to parent nets.
	////////////////////////////////////////////////

	for(i=0; i<biConnectedComponents->length; i++) {
		biConnectedComponent = biConnectedComponents->list[i];
		parentNet = parentNets[i];
		if(biConnectedComponents->length > 1) {
#ifdef BEN_DEBUG
			assert(parentNet != NULL);
#endif
			chain = chain_construct(net);
			for(j=1; j<biConnectedComponent->length; j++) {
				cactusEdge = biConnectedComponent->list[j-1];
				net = nets[mergedVertexIDs[cactusEdge->to->vertexID]];
#ifdef BEN_DEBUG
				assert(cactusEdge->to->vertexID != 0);
				assert(net != NULL);
#endif
				adjacencyComponent = adjacencyComponent_construct(parentNet, net);
				nets[cactusEdge->to->vertexID] = NULL; //defensive
				//Make link chain
				link_construct(net_getEnd(net, cactusEdgeToEndName(cactusEdge->rEdge, pinchGraph, names)),
								net_getEnd(net, cactusEdgeToEndName(biConnectedComponent->list[j], pinchGraph, names)),
								adjacencyComponent, chain);
			}
		}
	}
	logDebug("Constructed the chains and linked together the nets\n");

	net = nets[0];

	////////////////////////////////////////////////
	//Add surrounding atom caps to each chain
	////////////////////////////////////////////////

	addEnvelopingEnds(net);

	////////////////////////////////////////////////
	//Add nested ends to nets.
	////////////////////////////////////////////////

	destructList(addEnvelopedStubEnds(net));

	////////////////////////////////////////////////
	//Add adjacencies between ends.
	////////////////////////////////////////////////

	addAdjacenciesToEnds(net, pinchGraph, names);

	////////////////////////////////////////////////
	//Add adjacency components.
	////////////////////////////////////////////////

	addAdjacencyComponents(net, getUniqueName);

	////////////////////////////////////////////////
	//Ensure end trees are copied in base.
	////////////////////////////////////////////////

	copyEndTreePhylogenies(parentNet, net);

	////////////////////////////////////////////////
	//Clean up
	////////////////////////////////////////////////

	free(nets);
	free(mergedVertexIDs);
	free(vertexDiscoveryTimes);
	free(parentNets);
	destructList(biConnectedComponents);
	hashtable_destroy(chosenAtomsHash, FALSE, FALSE);

	return net;
}
