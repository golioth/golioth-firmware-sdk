def test_connect(board):
    # Reset board
    board.reset()

    # Confirm connection to Golioth
    assert None != board.wait_for_regex_in_line('Golioth CoAP client connected', timeout_s=60)
