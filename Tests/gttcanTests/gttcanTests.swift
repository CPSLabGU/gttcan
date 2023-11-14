import XCTest
@testable import gttcan

private let testWBName = "test-gttcan"

final class gttcanTests: XCTestCase {
    var ttcanptr = UnsafeMutablePointer<gttcan_t>.allocate(capacity: 1)

    static let localNode = UInt8(1)
    static let remoteNode = UInt8(2)
    static let slotDuration = UInt32(2_000_000) * 10
    static let scheduleLength = UInt8(4)

    deinit {
        ttcanptr.deallocate()
    }

    override func setUp() {
        GTTCAN_init(ttcanptr,
                    gttcanTests.localNode,
                    gttcanTests.slotDuration,
                    gttcanTests.scheduleLength,
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
        XCTAssertEqual(ttcanptr.pointee.scheduleLength, gttcanTests.scheduleLength)
        XCTAssertNotNil(ttcanptr.pointee.transmit_callback)
        XCTAssertNotNil(ttcanptr.pointee.set_timer_int_callback)
        XCTAssertNotNil(ttcanptr.pointee.read_value)
        XCTAssertNotNil(ttcanptr.pointee.write_value)
        XCTAssertEqual(ttcanptr.pointee.context_pointer, UnsafeMutableRawPointer(ttcanptr))
    }
}
