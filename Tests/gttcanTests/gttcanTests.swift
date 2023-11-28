import XCTest
@testable import gttcan

private let testWBName = "test-gttcan"

final class gttcanTests: XCTestCase {
    var ttcanptr = UnsafeMutablePointer<gttcan_t>.allocate(capacity: 1)

    static let localNode = UInt8(1)
    static let remoteNode = UInt8(2)
    static let slotDuration = UInt32(1_000) * 10
    static let globalScheduleLength = UInt16(4)
    static let canSlotOffset = 1480
    static let data1 = UInt32(1)

    deinit {
        ttcanptr.deallocate()
    }

    override func setUp() {
        GTTCAN_init(ttcanptr,
                    gttcanTests.localNode,
                    gttcanTests.slotDuration,
                    gttcanTests.globalScheduleLength,
                    { _, _, _ in },
                    { _, _ in },
                    { _, _ in 0 },
                    { _, _, _ in },
                    UnsafeMutableRawPointer(ttcanptr))
    }

    /// Test gttcan invariants
    func testInit() {
        XCTAssertEqual(ttcanptr.pointee.localNodeId, gttcanTests.localNode)
        XCTAssertEqual(ttcanptr.pointee.slotduration, gttcanTests.slotDuration)
        XCTAssertEqual(ttcanptr.pointee.globalScheduleLength, gttcanTests.globalScheduleLength)
        XCTAssertNotNil(ttcanptr.pointee.transmit_callback)
        XCTAssertNotNil(ttcanptr.pointee.set_timer_int_callback)
        XCTAssertNotNil(ttcanptr.pointee.read_value)
        XCTAssertNotNil(ttcanptr.pointee.write_value)
        XCTAssertEqual(ttcanptr.pointee.context_pointer, UnsafeMutableRawPointer(ttcanptr))
    }

    func testFTA() {
        XCTAssertEqual(ttcanptr.pointee.error_accumulator, 0)
        XCTAssertEqual(ttcanptr.pointee.slots_accumulated, 0)
        XCTAssertEqual(ttcanptr.pointee.lower_outlier, Int32.max)
        XCTAssertEqual(ttcanptr.pointee.upper_outlier, Int32.min)
        ttcanptr.pointee.transmitted = true
        XCTAssertEqual(GTTCAN_fta(ttcanptr), 0)
        ttcanptr.pointee.error_accumulator = 1
        ttcanptr.pointee.slots_accumulated = 1
        XCTAssertEqual(GTTCAN_fta(ttcanptr), 1)
        ttcanptr.pointee.error_accumulator = -3
        ttcanptr.pointee.slots_accumulated = 2
        XCTAssertEqual(GTTCAN_fta(ttcanptr), -1)
        ttcanptr.pointee.error_accumulator = 3
        ttcanptr.pointee.slots_accumulated = 4
        XCTAssertEqual(GTTCAN_fta(ttcanptr), 2)
        GTTCAN_accumulate_error(ttcanptr, -1)
        XCTAssertEqual(ttcanptr.pointee.lower_outlier, -1)
        XCTAssertEqual(ttcanptr.pointee.upper_outlier, -1)
        GTTCAN_accumulate_error(ttcanptr, 1)
        XCTAssertEqual(ttcanptr.pointee.lower_outlier, -1)
        XCTAssertEqual(ttcanptr.pointee.upper_outlier, 1)
        GTTCAN_accumulate_error(ttcanptr, -2)
        GTTCAN_accumulate_error(ttcanptr, 2)
        XCTAssertEqual(ttcanptr.pointee.lower_outlier, -2)
        XCTAssertEqual(ttcanptr.pointee.upper_outlier, 2)
        GTTCAN_accumulate_error(ttcanptr, -3)
        GTTCAN_accumulate_error(ttcanptr, 3)
        XCTAssertEqual(ttcanptr.pointee.lower_outlier, -3)
        XCTAssertEqual(ttcanptr.pointee.upper_outlier, 3)
        XCTAssertEqual(GTTCAN_fta(ttcanptr), 0)
        XCTAssertEqual(ttcanptr.pointee.lower_outlier, Int32.max)
        XCTAssertEqual(ttcanptr.pointee.upper_outlier, Int32.min)
        XCTAssertEqual(ttcanptr.pointee.error_accumulator, 0)
        XCTAssertEqual(ttcanptr.pointee.slots_accumulated, 0)
    }

    func testStateCorrection() {
        var ttcan = gttcan_t()
        GTTCAN_init(&ttcan,
                    gttcanTests.localNode,
                    gttcanTests.slotDuration,
                    gttcanTests.globalScheduleLength,
                    { _, _, _ in },
                    { _, _ in },
                    { _, _ in 0 },
                    { _, _, _ in },
                    UnsafeMutableRawPointer(ttcanptr))
        ttcan.transmitted = true
        // Simulate receiving from a slow node that is off by 1
        let slowNodeOffset = 1
        let expectedTimer = gttcanTests.slotDuration
        let timer = Int(expectedTimer) + gttcanTests.canSlotOffset + slowNodeOffset
        ttcan.action_time = UInt32(timer - gttcanTests.canSlotOffset)
        XCTAssertEqual(ttcan.action_time, expectedTimer + UInt32(slowNodeOffset))
        GTTCAN_process_frame(&ttcan, scheduleIndex(1) | gttcanTests.data1, 0)
        // We expect the accumulated error to be -1 (from our perspective)
        XCTAssertEqual(ttcan.error_accumulator, -1)
        XCTAssertEqual(ttcan.slots_accumulated, 1)
        XCTAssertEqual(ttcan.lower_outlier, -1)
        XCTAssertEqual(ttcan.upper_outlier, -1)
        for _ in 2...3 {
            ttcan.action_time = UInt32(timer - gttcanTests.canSlotOffset)
            GTTCAN_process_frame(&ttcan, scheduleIndex(1) | gttcanTests.data1, 0)
        }
        XCTAssertEqual(ttcan.error_accumulator, -3)
        XCTAssertEqual(ttcan.slots_accumulated, 3)
        XCTAssertEqual(ttcan.lower_outlier, -1)
        XCTAssertEqual(ttcan.upper_outlier, -1)
    }
}

private func scheduleIndex(_ slot: Int) -> UInt32 {
     UInt32(slot) << 14
}
