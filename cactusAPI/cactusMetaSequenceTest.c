#include "cactusGlobalsPrivate.h"

static NetDisk *netDisk;
static MetaEvent *metaEvent;
static MetaSequence *metaSequence;
static const char *sequenceString = "ACTGGCACTG";
static const char *headerString = ">one";

static bool nestedTest = 0;

static void cactusMetaSequenceTestTeardown() {
	if(!nestedTest && netDisk != NULL) {
		netDisk_destruct(netDisk);
		testCommon_deleteTemporaryNetDisk();
		netDisk = NULL;
		metaEvent = NULL;
		metaSequence = NULL;
	}
}

static void cactusMetaSequenceTestSetup() {
	if(!nestedTest) {
		cactusMetaSequenceTestTeardown();
		netDisk = netDisk_construct(testCommon_getTemporaryNetDisk());
		metaEvent = metaEvent_construct("ROOT", netDisk);
		metaSequence = metaSequence_construct(1, 10, sequenceString,
					   headerString, metaEvent_getName(metaEvent), netDisk);
	}
}

void testMetaSequence_getName(CuTest* testCase) {
	cactusMetaSequenceTestSetup();
	CuAssertTrue(testCase, metaSequence_getName(metaSequence) != NULL_NAME);
	CuAssertTrue(testCase, netDisk_getMetaSequence(netDisk, metaSequence_getName(metaSequence)) == metaSequence);
	cactusMetaSequenceTestTeardown();
}

void testMetaSequence_getStart(CuTest* testCase) {
	cactusMetaSequenceTestSetup();
	CuAssertIntEquals(testCase, 1, metaSequence_getStart(metaSequence));
	cactusMetaSequenceTestTeardown();
}

void testMetaSequence_getLength(CuTest* testCase) {
	cactusMetaSequenceTestSetup();
	CuAssertIntEquals(testCase, 10, metaSequence_getLength(metaSequence));
	cactusMetaSequenceTestTeardown();
}

void testMetaSequence_getEventName(CuTest* testCase) {
	cactusMetaSequenceTestSetup();
	CuAssertTrue(testCase, metaSequence_getEventName(metaSequence) == metaEvent_getName(metaEvent));
	cactusMetaSequenceTestTeardown();
}

void testMetaSequence_getString(CuTest* testCase) {
	cactusMetaSequenceTestSetup();
	//String is ACTGGCACTG
	CuAssertStrEquals(testCase, sequenceString, metaSequence_getString(metaSequence, 1, 10, 1)); //complete sequence
	CuAssertStrEquals(testCase, "TGGC", metaSequence_getString(metaSequence, 3, 4, 1)); //sub range
	CuAssertStrEquals(testCase, "", metaSequence_getString(metaSequence, 3, 0, 1)); //zero length sub range
	CuAssertStrEquals(testCase, "CAGTGCCAGT", metaSequence_getString(metaSequence, 1, 10, 0)); //reverse complement
	CuAssertStrEquals(testCase, "GCCA", metaSequence_getString(metaSequence, 3, 4, 0)); //sub range, reverse complement
	CuAssertStrEquals(testCase, "", metaSequence_getString(metaSequence, 3, 0, 0)); //zero length sub range on reverse strand
	cactusMetaSequenceTestTeardown();
}

void testMetaSequence_getHeader(CuTest* testCase) {
	cactusMetaSequenceTestSetup();
	CuAssertStrEquals(testCase, headerString, metaSequence_getHeader(metaSequence));
	cactusMetaSequenceTestTeardown();
}

void testMetaSequence_serialisation(CuTest* testCase) {
	cactusMetaSequenceTestSetup();
	int32_t i;
	Name name = metaSequence_getName(metaSequence);
	CuAssertTrue(testCase, netDisk_getMetaSequence(netDisk, name) == metaSequence);
	void *vA = binaryRepresentation_makeBinaryRepresentation(metaSequence,
			(void (*)(void *, void (*)(const void *, size_t, size_t)))metaSequence_writeBinaryRepresentation, &i);
	CuAssertTrue(testCase, i > 0);
	metaSequence_destruct(metaSequence);
	CuAssertTrue(testCase, netDisk_getMetaSequence(netDisk, name) == NULL);
	void *vA2 = vA;
	metaSequence = metaSequence_loadFromBinaryRepresentation(&vA2, netDisk);
	CuAssertTrue(testCase, name == metaSequence_getName(metaSequence));
	CuAssertStrEquals(testCase, headerString, metaSequence_getHeader(metaSequence));
	CuAssertTrue(testCase, netDisk_getMetaSequence(netDisk, name) == metaSequence);
	netDisk_write(netDisk);
	metaSequence_destruct(metaSequence);
	CuAssertTrue(testCase, netDisk_getMetaSequence(netDisk, name) != NULL);
	metaSequence = netDisk_getMetaSequence(netDisk, name);
	nestedTest = 1;
	testMetaSequence_getName(testCase);
	testMetaSequence_getStart(testCase);
	testMetaSequence_getLength(testCase);
	testMetaSequence_getEventName(testCase);
	testMetaSequence_getString(testCase);
	testMetaSequence_getHeader(testCase);
	nestedTest = 0;
	cactusMetaSequenceTestTeardown();
}

CuSuite* cactusMetaSequenceTestSuite(void) {
	CuSuite* suite = CuSuiteNew();
	SUITE_ADD_TEST(suite, testMetaSequence_getName);
	SUITE_ADD_TEST(suite, testMetaSequence_getStart);
	SUITE_ADD_TEST(suite, testMetaSequence_getLength);
	SUITE_ADD_TEST(suite, testMetaSequence_getEventName);
	SUITE_ADD_TEST(suite, testMetaSequence_getString);
	SUITE_ADD_TEST(suite, testMetaSequence_serialisation);
	SUITE_ADD_TEST(suite, testMetaSequence_getHeader);
	return suite;
}
