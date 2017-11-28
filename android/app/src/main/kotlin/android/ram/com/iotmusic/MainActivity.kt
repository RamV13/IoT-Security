package android.ram.com.iotmusic

import android.app.Activity
import android.os.Bundle
import android.os.Looper
import org.jetbrains.anko.*
import org.jetbrains.anko.sdk25.coroutines.onClick
import kotlin.concurrent.thread

class MainActivity : Activity() {

    companion object {
        private val IP = "http://104.131.124.11:3001"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        gridLayout {
            padding = dip(20)

            verticalLayout {
                button("Play") {
                    onClick {
                        thread {
                            try {
                                khttp.get(IP + "/play")
                            } catch (e: Exception) {
                                Looper.prepare()
                                toast("server error")
                            }
                        }
                    }
                }.lparams(width = 500, height = 750)

                button("Skip") {
                    onClick {
                        thread {
                            try {
                                khttp.get(IP + "/skip")
                            } catch (e: Exception) {
                                Looper.prepare()
                                toast("server error")
                            }
                        }
                    }
                }.lparams(width = 500, height = 750)
            }

            verticalLayout {
                button("Pause") {
                    onClick {
                        thread {
                            try {
                                khttp.get(IP + "/pause")
                            } catch (e: Exception) {
                                Looper.prepare()
                                toast("server error")
                            }
                        }
                    }
                }.lparams(width = 500, height = 750)

                button("Back") {
                    onClick {
                        thread {
                            try {
                                khttp.get(IP + "/prev")
                            } catch (e: Exception) {
                                Looper.prepare()
                                toast("server error")
                            }
                        }
                    }
                }.lparams(width = 500, height = 750)
            }
        }
    }

}
